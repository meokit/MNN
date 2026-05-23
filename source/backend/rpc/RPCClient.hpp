//
//  RPCClient.hpp
//  MNN
//

#ifndef MNN_SOURCE_BACKEND_RPC_RPCCLIENT_HPP
#define MNN_SOURCE_BACKEND_RPC_RPCCLIENT_HPP

#include <memory>
#include <string>
#include <vector>
#include <MNN/RPC.h>
#include "RPCProtocol.hpp"
#include "RPCTransport.hpp"

namespace MNN {
namespace RPC {

struct NodeConfig {
    std::string host;
    uint16_t port = 0;
    MNNForwardType backendType = MNN_FORWARD_CPU;
    int32_t deviceId = 0;
    float weight = 1.0f;
    std::string cachePath;
};

struct SegmentConfig {
    std::string name;
    int32_t opStart = -1;
    int32_t opEnd = -1;
    int32_t layerStart = -1;
    int32_t layerEnd = -1;
    uint32_t nodeIndex = 0;
    uint32_t flags = 0;
};

struct ClientConfig {
    uint32_t configVersion = MNN_RPC_CONFIG_VERSION;
    uint32_t protocolVersion = MNN_RPC_PROTOCOL_VERSION;
    uint32_t flags = 0;
    uint32_t connectTimeoutMs = 5000;
    uint32_t ioTimeoutMs = 5000;
    uint32_t maxPacketSize = 8 * 1024 * 1024;
    std::string authToken;
    std::vector<NodeConfig> nodes;
    std::vector<SegmentConfig> segments;

    static bool from(const MNNRPCClientConfig* src, ClientConfig& dst, std::string& error);
};

class Client {
public:
    explicit Client(const ClientConfig& config);

    bool connect();
    bool hello();
    bool ping(uint64_t* timestamp = nullptr);
    bool getCapabilities(std::vector<MNNRPCDeviceCapability>& capabilities);
    const std::string& lastError() const;
    const ClientConfig& config() const;

private:
    bool request(uint16_t command, const void* request, size_t requestSize, std::vector<uint8_t>& response, uint32_t flags = 0);
    void setError(const std::string& error);

private:
    ClientConfig mConfig;
    std::unique_ptr<Socket> mSocket;
    std::string mLastError;
    uint64_t mRequestId = 1;
};

} // namespace RPC
} // namespace MNN

#endif
