//
//  RemoteRuntime.cpp
//  MNN
//

#include "RemoteBackend.hpp"
#include "core/Backend.hpp"
#include "core/Macro.h"
#include "RPCClient.hpp"

namespace MNN {
namespace RPC {

class RemoteRuntime : public Runtime {
public:
    explicit RemoteRuntime(const ClientConfig& config) : mConfig(config) {
        if (!config.nodes.empty()) {
            mClient.reset(new Client(config));
        }
    }

    Backend* onCreate(const BackendConfig* config = nullptr, Backend* origin = nullptr) const override {
        (void)origin;
        if (config == nullptr) {
            return nullptr;
        }
        auto bn = new RemoteBackend(mClient);
        bn->setRuntime(this);
        return bn;
    }

    void onGabageCollect(int level) override {
        (void)level;
    }

    bool onMeasure(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs, const MNN::Op* op,
                   OpInfo& dstInfo) const override {
        (void)inputs;
        (void)outputs;
        (void)op;
        dstInfo.initCostLong = false;
        dstInfo.exeutionCost = 1.0f;
        dstInfo.initCost = 0.0f;
        return false;
    }

    int onGetRuntimeStatus(RuntimeStatus statusEnum) const override {
        (void)statusEnum;
        return 0;
    }

    std::shared_ptr<Client> client() const {
        return mClient;
    }

private:
    ClientConfig mConfig;
    std::shared_ptr<Client> mClient;
};

class RemoteRuntimeCreator : public RuntimeCreator {
public:
    Runtime* onCreate(const Backend::Info& info) const override {
        ClientConfig config;
        if (info.user == nullptr || info.user->sharedContext == nullptr) {
            return new RemoteRuntime(config);
        }
        std::string error;
        if (!ClientConfig::from(reinterpret_cast<const MNNRPCClientConfig*>(info.user->sharedContext), config, error)) {
            MNN_ERROR("RPC runtime config error: %s\n", error.c_str());
            return nullptr;
        }
        std::shared_ptr<Client> client(new Client(config));
        if (!client->connect() || !client->hello()) {
            MNN_ERROR("RPC runtime handshake failed: %s\n", client->lastError().c_str());
            return nullptr;
        }
        std::vector<MNNRPCDeviceCapability> capabilities;
        if (!client->getCapabilities(capabilities)) {
            MNN_ERROR("RPC capability request failed: %s\n", client->lastError().c_str());
            return nullptr;
        }
        auto runtime = new RemoteRuntime(config);
        return runtime;
    }

    bool onValid(Backend::Info& info) const override {
        (void)info;
        return true;
    }
};

} // namespace RPC

void registerRPCRuntimeCreator() {
    MNNInsertExtraRuntimeCreator(MNN_FORWARD_USER_2, new RPC::RemoteRuntimeCreator, false);
}
} // namespace MNN
