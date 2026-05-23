//
//  RPCProtocol.hpp
//  MNN
//

#ifndef MNN_SOURCE_BACKEND_RPC_RPCPROTOCOL_HPP
#define MNN_SOURCE_BACKEND_RPC_RPCPROTOCOL_HPP

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <MNN/RPC.h>

namespace MNN {
namespace RPC {

static const uint32_t kMagic = 0x4D4E4E52u; // MNNR

#pragma pack(push, 1)
struct PacketHeader {
    uint32_t magic = kMagic;
    uint16_t version = MNN_RPC_PROTOCOL_VERSION;
    uint16_t command = 0;
    uint32_t flags = 0;
    uint64_t requestId = 0;
    uint32_t payloadSize = 0;
    uint32_t reserved = 0;
};

struct StatusPayload {
    uint32_t status = MNN_RPC_STATUS_OK;
};

struct HelloRequest {
    uint32_t protocolVersion = MNN_RPC_PROTOCOL_VERSION;
    uint32_t flags = 0;
};

struct HelloResponse {
    uint32_t status = MNN_RPC_STATUS_OK;
    uint32_t protocolVersion = MNN_RPC_PROTOCOL_VERSION;
    uint32_t flags = 0;
};

struct PingPayload {
    uint64_t timestamp = 0;
};

struct CapabilityListHeader {
    uint32_t status = MNN_RPC_STATUS_OK;
    uint32_t count = 0;
};

struct GraphRegisterRequest {
    uint64_t graphUid = 0;
    uint64_t segmentUid = 0;
    uint64_t shapeSignature = 0;
    uint32_t opCount = 0;
    uint32_t tensorCount = 0;
    uint32_t flags = 0;
    uint32_t reserved = 0;
};

struct GraphOperationResponse {
    uint32_t status = MNN_RPC_STATUS_OK;
    uint32_t flags = 0;
    uint64_t graphUid = 0;
    uint64_t segmentUid = 0;
};

struct WeightPushRequest {
    uint64_t tensorUid = 0;
    uint64_t byteSize = 0;
    uint32_t dataType = 0;
    uint32_t dimensions = 0;
    uint32_t flags = 0;
    uint32_t reserved = 0;
};

struct StatsPayload {
    uint32_t status = MNN_RPC_STATUS_OK;
    uint32_t graphCount = 0;
    uint32_t weightCount = 0;
    uint32_t reserved = 0;
    uint64_t graphRegisterCount = 0;
    uint64_t graphCacheHitCount = 0;
    uint64_t weightPushCount = 0;
    uint64_t weightCacheHitCount = 0;
};
#pragma pack(pop)

std::vector<uint8_t> encodeHeader(const PacketHeader& header);
bool decodeHeader(const uint8_t* data, size_t size, PacketHeader& header);
std::string commandName(uint16_t command);

} // namespace RPC
} // namespace MNN

#endif
