//
//  RemoteBackend.hpp
//  MNN
//

#ifndef MNN_SOURCE_BACKEND_RPC_REMOTEBACKEND_HPP
#define MNN_SOURCE_BACKEND_RPC_REMOTEBACKEND_HPP

#include <memory>
#include <string>
#include "core/Backend.hpp"
#include "core/Execution.hpp"
#include "RPCClient.hpp"

namespace MNN {
namespace RPC {

class RemoteExecution;

class RemoteBackend : public Backend {
public:
    explicit RemoteBackend(std::shared_ptr<Client> client);
    ~RemoteBackend() override = default;

    Execution* onCreate(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs, const MNN::Op* op) override;
    ErrorCode onResizeEnd() override;
    void onExecuteBegin() const override;
    void onExecuteEnd() const override;
    MemObj* onAcquire(const Tensor* tensor, StorageType storageType) override;
    bool onClearBuffer() override;
    void onCopyBuffer(const Tensor* srcTensor, const Tensor* dstTensor) const override;
    const Runtime* getRuntime() override;
    std::shared_ptr<Client> client() const;
    void setRuntime(const Runtime* runtime);
    void setLastError(const std::string& error);
    const std::string& lastError() const;

private:
    class RemoteMemObj : public MemObj {
    public:
        explicit RemoteMemObj(size_t size);
        ~RemoteMemObj() override;
        MemChunk chunk() override;
    private:
        void* mPtr = nullptr;
    };

private:
    std::shared_ptr<Client> mClient;
    const Runtime* mRuntime = nullptr;
    mutable std::string mLastError;
};

class RemoteExecution : public Execution {
public:
    RemoteExecution(Backend* backend, std::shared_ptr<Client> client, const GraphDescriptor& graph);
    ~RemoteExecution() override = default;
    ErrorCode onResize(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs) override;
    ErrorCode onExecute(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs) override;

private:
    std::shared_ptr<Client> mClient;
    GraphDescriptor mGraph;
    bool mRegistered = false;
};

} // namespace RPC
} // namespace MNN

#endif
