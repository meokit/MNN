//
//  RPCServer.cpp
//  MNN
//

#include "RPCServer.hpp"
#include <string.h>

namespace MNN {
namespace RPC {

static const uint32_t kCacheHitFlag = 1u;

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
        if (header.payloadSize > mConfig.maxPacketSize) {
            mLastError = "RPC payload exceeds server max packet size";
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
            case MNN_RPC_CMD_REGISTER_GRAPH:
                if (!handleRegisterGraph(socket, header, payload)) {
                    return false;
                }
                break;
            case MNN_RPC_CMD_FREE_GRAPH:
                if (!handleFreeGraph(socket, header, payload)) {
                    return false;
                }
                break;
            case MNN_RPC_CMD_PUSH_WEIGHT:
                if (!handlePushWeight(socket, header, payload)) {
                    return false;
                }
                break;
            case MNN_RPC_CMD_GC:
                mGraphCache.clear();
                mTensorCache.clear();
                if (!handleGetStats(socket, header)) {
                    return false;
                }
                break;
            case MNN_RPC_CMD_GET_STATS:
                if (!handleGetStats(socket, header)) {
                    return false;
                }
                break;
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

bool Server::handleRegisterGraph(Socket& socket, const PacketHeader& header, const std::vector<uint8_t>& payload) {
    GraphOperationResponse response;
    if (payload.size() != sizeof(GraphRegisterRequest)) {
        response.status = MNN_RPC_STATUS_BAD_REQUEST;
        return sendResponse(socket, header.command, header.requestId, &response, sizeof(response));
    }
    GraphRegisterRequest request;
    ::memcpy(&request, payload.data(), sizeof(request));
    GraphCacheEntry entry;
    entry.graphUid = request.graphUid;
    entry.segmentUid = request.segmentUid;
    entry.shapeSignature = request.shapeSignature;
    entry.opCount = request.opCount;
    entry.tensorCount = request.tensorCount;
    entry.flags = request.flags;
    GraphCacheEntry stored;
    bool inserted = mGraphCache.insertOrRefresh(entry, &stored);
    mStats.graphRegisterCount += 1;
    if (!inserted) {
        mStats.graphCacheHitCount += 1;
        response.flags |= kCacheHitFlag;
    }
    response.graphUid = stored.graphUid;
    response.segmentUid = stored.segmentUid;
    return sendResponse(socket, header.command, header.requestId, &response, sizeof(response));
}

bool Server::handleFreeGraph(Socket& socket, const PacketHeader& header, const std::vector<uint8_t>& payload) {
    GraphOperationResponse response;
    if (payload.size() != sizeof(GraphRegisterRequest)) {
        response.status = MNN_RPC_STATUS_BAD_REQUEST;
        return sendResponse(socket, header.command, header.requestId, &response, sizeof(response));
    }
    GraphRegisterRequest request;
    ::memcpy(&request, payload.data(), sizeof(request));
    if (!mGraphCache.erase(request.graphUid)) {
        response.status = MNN_RPC_STATUS_BAD_REQUEST;
    }
    response.graphUid = request.graphUid;
    return sendResponse(socket, header.command, header.requestId, &response, sizeof(response));
}

bool Server::handlePushWeight(Socket& socket, const PacketHeader& header, const std::vector<uint8_t>& payload) {
    GraphOperationResponse response;
    if (payload.size() < sizeof(WeightPushRequest)) {
        response.status = MNN_RPC_STATUS_BAD_REQUEST;
        return sendResponse(socket, header.command, header.requestId, &response, sizeof(response));
    }
    WeightPushRequest request;
    ::memcpy(&request, payload.data(), sizeof(request));
    auto dataSize = payload.size() - sizeof(request);
    if (dataSize != request.byteSize) {
        response.status = MNN_RPC_STATUS_BAD_REQUEST;
        return sendResponse(socket, header.command, header.requestId, &response, sizeof(response));
    }
    TensorCacheEntry entry;
    entry.tensorUid = request.tensorUid;
    entry.byteSize = request.byteSize;
    entry.dataType = request.dataType;
    entry.dimensions = request.dimensions;
    entry.flags = request.flags;
    entry.data.resize(dataSize);
    if (dataSize > 0) {
        ::memcpy(entry.data.data(), payload.data() + sizeof(request), dataSize);
    }
    TensorCacheEntry stored;
    bool inserted = mTensorCache.insertOrRefresh(entry, &stored);
    mStats.weightPushCount += 1;
    if (!inserted) {
        mStats.weightCacheHitCount += 1;
        response.flags |= kCacheHitFlag;
    }
    response.graphUid = stored.tensorUid;
    return sendResponse(socket, header.command, header.requestId, &response, sizeof(response));
}

bool Server::handleGetStats(Socket& socket, const PacketHeader& header) {
    StatsPayload response;
    response.graphCount = static_cast<uint32_t>(mGraphCache.size());
    response.weightCount = static_cast<uint32_t>(mTensorCache.size());
    response.graphRegisterCount = mStats.graphRegisterCount;
    response.graphCacheHitCount = mStats.graphCacheHitCount;
    response.weightPushCount = mStats.weightPushCount;
    response.weightCacheHitCount = mStats.weightCacheHitCount;
    return sendResponse(socket, header.command, header.requestId, &response, sizeof(response));
}

} // namespace RPC
} // namespace MNN
