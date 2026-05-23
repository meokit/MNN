//
//  RPCServer.hpp
//  MNN
//

#ifndef MNN_SOURCE_BACKEND_RPC_RPCSERVER_HPP
#define MNN_SOURCE_BACKEND_RPC_RPCSERVER_HPP

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <MNN/RPC.h>
#include "RPCProtocol.hpp"
#include "RPCTransport.hpp"

namespace MNN {
namespace RPC {

struct ServerConfig {
    std::string host = "0.0.0.0";
    int port = 0;
    MNNForwardType backendType = MNN_FORWARD_CPU;
    int deviceId = 0;
    int memoryMB = 0;
    int computeUnits = 0;
    uint32_t flags = 0;
    int acceptTimeoutMs = 200;
};

class Server {
public:
    explicit Server(const ServerConfig& config);
    bool start();
    void stop();
    bool serveOnce();
    bool valid() const;
    std::string lastError() const;

private:
    bool handleClient(Socket& socket);
    bool sendResponse(Socket& socket, uint16_t command, uint64_t requestId, const void* payload, size_t payloadSize, uint32_t flags = 0);

private:
    ServerConfig mConfig;
    std::unique_ptr<Socket> mListener;
    std::string mLastError;
    std::atomic<bool> mRunning{false};
};

} // namespace RPC
} // namespace MNN

#endif
