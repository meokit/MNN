//
//  RPCClient.cpp
//  MNN
//

#include "RPCClient.hpp"
#include <chrono>
#include <string.h>

namespace MNN {
namespace RPC {

namespace {
static std::string safeString(const char* str) {
    return str == nullptr ? std::string() : std::string(str);
}
}

bool ClientConfig::from(const MNNRPCClientConfig* src, ClientConfig& dst, std::string& error) {
    if (src == nullptr) {
        error = "RPC config is null";
        return false;
    }
    if (src->structSize < sizeof(MNNRPCClientConfig)) {
        error = "RPC config structSize is too small";
        return false;
    }
    if (src->configVersion != MNN_RPC_CONFIG_VERSION) {
        error = "RPC config version mismatch";
        return false;
    }
    if (src->protocolVersion != MNN_RPC_PROTOCOL_VERSION) {
        error = "RPC protocol version mismatch";
        return false;
    }
    if (src->nodeCount == 0 || src->nodes == nullptr) {
        error = "RPC config requires at least one node";
        return false;
    }
    dst.configVersion = src->configVersion;
    dst.protocolVersion = src->protocolVersion;
    dst.flags = src->flags;
    dst.connectTimeoutMs = src->connectTimeoutMs == 0 ? 5000 : src->connectTimeoutMs;
    dst.ioTimeoutMs = src->ioTimeoutMs == 0 ? 5000 : src->ioTimeoutMs;
    dst.maxPacketSize = src->maxPacketSize == 0 ? 8 * 1024 * 1024 : src->maxPacketSize;
    dst.authToken = safeString(src->authToken);
    dst.nodes.clear();
    dst.segments.clear();
    dst.nodes.reserve(src->nodeCount);
    for (size_t i = 0; i < src->nodeCount; ++i) {
        const auto& in = src->nodes[i];
        if (in.host == nullptr || in.port == 0) {
            error = "RPC node host / port is invalid";
            return false;
        }
        NodeConfig out;
        out.host = in.host;
        out.port = in.port;
        out.backendType = in.backendType;
        out.deviceId = in.deviceId;
        out.weight = in.weight <= 0.0f ? 1.0f : in.weight;
        out.cachePath = safeString(in.cachePath);
        dst.nodes.emplace_back(std::move(out));
    }
    dst.segments.reserve(src->segmentCount);
    for (size_t i = 0; i < src->segmentCount; ++i) {
        const auto& in = src->segments[i];
        SegmentConfig out;
        out.name = safeString(in.name);
        out.opStart = in.opStart;
        out.opEnd = in.opEnd;
        out.layerStart = in.layerStart;
        out.layerEnd = in.layerEnd;
        out.nodeIndex = in.nodeIndex;
        out.flags = in.flags;
        if (out.nodeIndex >= dst.nodes.size()) {
            error = "RPC segment node index is out of range";
            return false;
        }
        dst.segments.emplace_back(std::move(out));
    }
    return true;
}

Client::Client(const ClientConfig& config) : mConfig(config) {
}

bool Client::connect() {
    if (mSocket && mSocket->valid()) {
        return true;
    }
    mSocket.reset(new Socket);
    const auto& node = mConfig.nodes.front();
    if (!mSocket->connect(node.host, node.port, static_cast<int>(mConfig.connectTimeoutMs))) {
        setError(mSocket->lastError());
        mSocket.reset();
        return false;
    }
    mSocket->setTimeout(static_cast<int>(mConfig.ioTimeoutMs));
    return true;
}

bool Client::hello() {
    HelloRequest request;
    request.protocolVersion = mConfig.protocolVersion;
    request.flags = mConfig.flags;
    std::vector<uint8_t> response;
    if (!this->request(MNN_RPC_CMD_HELLO, &request, sizeof(request), response)) {
        return false;
    }
    if (response.size() != sizeof(HelloResponse)) {
        setError("Unexpected HELLO response size");
        return false;
    }
    HelloResponse hello;
    ::memcpy(&hello, response.data(), sizeof(hello));
    if (hello.status != MNN_RPC_STATUS_OK) {
        setError("HELLO rejected by server");
        return false;
    }
    if (hello.protocolVersion != mConfig.protocolVersion) {
        setError("HELLO protocol mismatch");
        return false;
    }
    return true;
}

bool Client::ping(uint64_t* timestamp) {
    PingPayload request;
    request.timestamp = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::vector<uint8_t> response;
    if (!this->request(MNN_RPC_CMD_PING, &request, sizeof(request), response)) {
        return false;
    }
    if (response.size() != sizeof(PingPayload)) {
        setError("Unexpected PING response size");
        return false;
    }
    PingPayload pong;
    ::memcpy(&pong, response.data(), sizeof(pong));
    if (timestamp != nullptr) {
        *timestamp = pong.timestamp;
    }
    return true;
}

bool Client::getCapabilities(std::vector<MNNRPCDeviceCapability>& capabilities) {
    std::vector<uint8_t> response;
    if (!this->request(MNN_RPC_CMD_GET_CAPABILITIES, nullptr, 0, response)) {
        return false;
    }
    if (response.size() < sizeof(CapabilityListHeader)) {
        setError("Unexpected capability response size");
        return false;
    }
    CapabilityListHeader header;
    ::memcpy(&header, response.data(), sizeof(header));
    if (header.status != MNN_RPC_STATUS_OK) {
        setError("Capability request failed");
        return false;
    }
    auto expected = sizeof(CapabilityListHeader) + static_cast<size_t>(header.count) * sizeof(MNNRPCDeviceCapability);
    if (response.size() != expected) {
        setError("Capability payload size mismatch");
        return false;
    }
    capabilities.resize(header.count);
    if (header.count > 0) {
        ::memcpy(capabilities.data(), response.data() + sizeof(header), header.count * sizeof(MNNRPCDeviceCapability));
    }
    return true;
}

bool Client::registerGraph(const GraphDescriptor& graph, bool* cacheHit) {
    GraphRegisterRequest request;
    request.graphUid = graph.graphUid;
    request.segmentUid = graph.segmentUid;
    request.shapeSignature = graph.shapeSignature;
    request.opCount = graph.opCount;
    request.tensorCount = graph.tensorCount;
    request.flags = graph.flags;
    std::vector<uint8_t> response;
    if (!this->request(MNN_RPC_CMD_REGISTER_GRAPH, &request, sizeof(request), response)) {
        return false;
    }
    if (response.size() != sizeof(GraphOperationResponse)) {
        setError("Unexpected REGISTER_GRAPH response size");
        return false;
    }
    GraphOperationResponse payload;
    ::memcpy(&payload, response.data(), sizeof(payload));
    if (payload.status != MNN_RPC_STATUS_OK) {
        setError("REGISTER_GRAPH rejected by server");
        return false;
    }
    if (cacheHit != nullptr) {
        *cacheHit = (payload.flags & 1u) != 0;
    }
    return true;
}

bool Client::freeGraph(uint64_t graphUid) {
    GraphRegisterRequest request;
    request.graphUid = graphUid;
    std::vector<uint8_t> response;
    if (!this->request(MNN_RPC_CMD_FREE_GRAPH, &request, sizeof(request), response)) {
        return false;
    }
    if (response.size() != sizeof(GraphOperationResponse)) {
        setError("Unexpected FREE_GRAPH response size");
        return false;
    }
    GraphOperationResponse payload;
    ::memcpy(&payload, response.data(), sizeof(payload));
    if (payload.status != MNN_RPC_STATUS_OK) {
        setError("FREE_GRAPH rejected by server");
        return false;
    }
    return true;
}

bool Client::pushWeight(const TensorDescriptor& tensor, const void* data, size_t size, bool* cacheHit) {
    if ((data == nullptr && size > 0) || size != tensor.byteSize) {
        setError("Invalid PUSH_WEIGHT payload");
        return false;
    }
    std::vector<uint8_t> request(sizeof(WeightPushRequest) + size);
    WeightPushRequest header;
    header.tensorUid = tensor.tensorUid;
    header.byteSize = tensor.byteSize;
    header.dataType = tensor.dataType;
    header.dimensions = tensor.dimensions;
    header.flags = tensor.flags;
    ::memcpy(request.data(), &header, sizeof(header));
    if (size > 0) {
        ::memcpy(request.data() + sizeof(header), data, size);
    }
    std::vector<uint8_t> response;
    if (!this->request(MNN_RPC_CMD_PUSH_WEIGHT, request.data(), request.size(), response)) {
        return false;
    }
    if (response.size() != sizeof(GraphOperationResponse)) {
        setError("Unexpected PUSH_WEIGHT response size");
        return false;
    }
    GraphOperationResponse payload;
    ::memcpy(&payload, response.data(), sizeof(payload));
    if (payload.status != MNN_RPC_STATUS_OK) {
        setError("PUSH_WEIGHT rejected by server");
        return false;
    }
    if (cacheHit != nullptr) {
        *cacheHit = (payload.flags & 1u) != 0;
    }
    return true;
}

bool Client::getStats(RuntimeStats& stats) {
    std::vector<uint8_t> response;
    if (!this->request(MNN_RPC_CMD_GET_STATS, nullptr, 0, response)) {
        return false;
    }
    if (response.size() != sizeof(StatsPayload)) {
        setError("Unexpected GET_STATS response size");
        return false;
    }
    StatsPayload payload;
    ::memcpy(&payload, response.data(), sizeof(payload));
    if (payload.status != MNN_RPC_STATUS_OK) {
        setError("GET_STATS rejected by server");
        return false;
    }
    stats.graphCount = payload.graphCount;
    stats.weightCount = payload.weightCount;
    stats.graphRegisterCount = payload.graphRegisterCount;
    stats.graphCacheHitCount = payload.graphCacheHitCount;
    stats.weightPushCount = payload.weightPushCount;
    stats.weightCacheHitCount = payload.weightCacheHitCount;
    return true;
}

const std::string& Client::lastError() const {
    return mLastError;
}

const ClientConfig& Client::config() const {
    return mConfig;
}

bool Client::request(uint16_t command, const void* request, size_t requestSize, std::vector<uint8_t>& response, uint32_t flags) {
    if (!connect()) {
        return false;
    }
    PacketHeader header;
    header.command = command;
    header.flags = flags;
    header.requestId = mRequestId++;
    header.payloadSize = static_cast<uint32_t>(requestSize);
    auto encoded = encodeHeader(header);
    if (!mSocket->sendAll(encoded.data(), encoded.size())) {
        setError(mSocket->lastError());
        return false;
    }
    if (requestSize > 0 && !mSocket->sendAll(request, requestSize)) {
        setError(mSocket->lastError());
        return false;
    }
    std::vector<uint8_t> headerBuffer(sizeof(PacketHeader));
    if (!mSocket->recvAll(headerBuffer.data(), headerBuffer.size())) {
        setError(mSocket->lastError());
        return false;
    }
    PacketHeader responseHeader;
    if (!decodeHeader(headerBuffer.data(), headerBuffer.size(), responseHeader)) {
        setError("Invalid RPC response header");
        return false;
    }
    if (responseHeader.payloadSize > mConfig.maxPacketSize) {
        setError("RPC response payload exceeds configured max packet size");
        return false;
    }
    response.resize(responseHeader.payloadSize);
    if (responseHeader.payloadSize > 0 && !mSocket->recvAll(response.data(), response.size())) {
        setError(mSocket->lastError());
        return false;
    }
    return true;
}

void Client::setError(const std::string& error) {
    mLastError = error;
}

} // namespace RPC
} // namespace MNN
