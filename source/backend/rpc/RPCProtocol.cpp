//
//  RPCProtocol.cpp
//  MNN
//

#include "RPCProtocol.hpp"
#include <string.h>

namespace MNN {
namespace RPC {

std::vector<uint8_t> encodeHeader(const PacketHeader& header) {
    std::vector<uint8_t> buffer(sizeof(PacketHeader));
    ::memcpy(buffer.data(), &header, sizeof(PacketHeader));
    return buffer;
}

bool decodeHeader(const uint8_t* data, size_t size, PacketHeader& header) {
    if (data == nullptr || size < sizeof(PacketHeader)) {
        return false;
    }
    ::memcpy(&header, data, sizeof(PacketHeader));
    return header.magic == kMagic;
}

std::string commandName(uint16_t command) {
    switch (command) {
        case MNN_RPC_CMD_HELLO:
            return "HELLO";
        case MNN_RPC_CMD_GET_CAPABILITIES:
            return "GET_CAPABILITIES";
        case MNN_RPC_CMD_PING:
            return "PING";
        case MNN_RPC_CMD_REGISTER_GRAPH:
            return "REGISTER_GRAPH";
        case MNN_RPC_CMD_FREE_GRAPH:
            return "FREE_GRAPH";
        case MNN_RPC_CMD_RESIZE_GRAPH:
            return "RESIZE_GRAPH";
        case MNN_RPC_CMD_RECOMPUTE_GRAPH:
            return "RECOMPUTE_GRAPH";
        case MNN_RPC_CMD_PUSH_WEIGHT:
            return "PUSH_WEIGHT";
        case MNN_RPC_CMD_PUSH_INPUT:
            return "PUSH_INPUT";
        case MNN_RPC_CMD_PULL_OUTPUT:
            return "PULL_OUTPUT";
        case MNN_RPC_CMD_PUSH_KV:
            return "PUSH_KV";
        case MNN_RPC_CMD_PULL_KV:
            return "PULL_KV";
        case MNN_RPC_CMD_RUN_SEGMENT:
            return "RUN_SEGMENT";
        case MNN_RPC_CMD_RUN_SEGMENT_ASYNC:
            return "RUN_SEGMENT_ASYNC";
        case MNN_RPC_CMD_WAIT:
            return "WAIT";
        case MNN_RPC_CMD_CANCEL:
            return "CANCEL";
        case MNN_RPC_CMD_GC:
            return "GC";
        case MNN_RPC_CMD_GET_STATS:
            return "GET_STATS";
        default:
            return "UNKNOWN";
    }
}

} // namespace RPC
} // namespace MNN
