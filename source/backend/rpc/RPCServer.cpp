//
//  RPCServer.cpp
//  MNN
//

#include "RPCServer.hpp"
#include <string.h>

namespace MNN {
namespace RPC {

Server::Server(const ServerConfig& config) : mConfig(config) {
}

bool Server::start() {
    if (mListener && mListener->valid()) {
        return true;
    }
    mListener.reset(new Socket);
    if (!mListener->bindAndListen(mConfig.host, mConfig.port, 16)) {
        mLastError = mListener->lastError();
        mListener.reset();
        return false;
    }
    mRunning = true;
    return true;
}

void Server::stop() {
    mRunning = false;
    if (mListener) {
        mListener->close();
    }
}

bool Server::serveOnce() {
    if (!start()) {
        return false;
    }
    auto client = mListener->accept(mConfig.acceptTimeoutMs);
    if (!client) {
        return mRunning;
    }
    return handleClient(*client);
}

bool Server::valid() const {
    return mListener && mListener->valid();
}

std::string Server::lastError() const {
    return mLastError;
}

bool Server::handleClient(Socket& socket) {
    std::vector<uint8_t> headerBuffer(sizeof(PacketHeader));
    while (mRunning.load()) {
        if (!socket.recvAll(headerBuffer.data(), headerBuffer.size())) {
            return true;
        }
        PacketHeader header;
        if (!decodeHeader(headerBuffer.data(), headerBuffer.size(), header)) {
            mLastError = "Invalid RPC header";
            return false;
        }
        std::vector<uint8_t> payload(header.payloadSize);
        if (header.payloadSize > 0 && !socket.recvAll(payload.data(), payload.size())) {
            mLastError = socket.lastError();
            return false;
        }
        switch (header.command) {
            case MNN_RPC_CMD_HELLO: {
                HelloResponse response;
                response.status = MNN_RPC_STATUS_OK;
                response.protocolVersion = MNN_RPC_PROTOCOL_VERSION;
                response.flags = mConfig.flags;
                if (!sendResponse(socket, header.command, header.requestId, &response, sizeof(response))) {
                    return false;
                }
                break;
            }
            case MNN_RPC_CMD_PING: {
                PingPayload pong;
                if (payload.size() == sizeof(PingPayload)) {
                    ::memcpy(&pong, payload.data(), sizeof(pong));
                }
                if (!sendResponse(socket, header.command, header.requestId, &pong, sizeof(pong))) {
                    return false;
                }
                break;
            }
            case MNN_RPC_CMD_GET_CAPABILITIES: {
                CapabilityListHeader capHeader;
                capHeader.status = MNN_RPC_STATUS_OK;
                capHeader.count = 1;
                MNNRPCDeviceCapability capability;
                capability.backendType = mConfig.backendType;
                capability.deviceId = mConfig.deviceId;
                capability.memoryMB = mConfig.memoryMB;
                capability.computeUnits = mConfig.computeUnits;
                capability.flags = mConfig.flags;
                std::vector<uint8_t> response(sizeof(capHeader) + sizeof(capability));
                ::memcpy(response.data(), &capHeader, sizeof(capHeader));
                ::memcpy(response.data() + sizeof(capHeader), &capability, sizeof(capability));
                if (!sendResponse(socket, header.command, header.requestId, response.data(), response.size())) {
                    return false;
                }
                break;
            }
            default: {
                StatusPayload status;
                status.status = MNN_RPC_STATUS_UNSUPPORTED;
                if (!sendResponse(socket, header.command, header.requestId, &status, sizeof(status))) {
                    return false;
                }
                break;
            }
        }
    }
    return true;
}

bool Server::sendResponse(Socket& socket, uint16_t command, uint64_t requestId, const void* payload, size_t payloadSize, uint32_t flags) {
    PacketHeader header;
    header.command = command;
    header.flags = flags;
    header.requestId = requestId;
    header.payloadSize = static_cast<uint32_t>(payloadSize);
    auto encoded = encodeHeader(header);
    if (!socket.sendAll(encoded.data(), encoded.size())) {
        mLastError = socket.lastError();
        return false;
    }
    if (payloadSize > 0 && !socket.sendAll(payload, payloadSize)) {
        mLastError = socket.lastError();
        return false;
    }
    return true;
}

} // namespace RPC
} // namespace MNN
