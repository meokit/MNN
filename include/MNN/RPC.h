//
//  RPC.h
//  MNN
//
//  Created by MNN on 2026/05/23.
//

#ifndef MNN_RPC_H
#define MNN_RPC_H

#include <stddef.h>
#include <stdint.h>
#include "MNNDefine.h"
#include "MNNForwardType.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MNN_RPC_PROTOCOL_VERSION 1u
#define MNN_RPC_CONFIG_VERSION 1u

typedef enum {
    MNN_RPC_CMD_HELLO = 1,
    MNN_RPC_CMD_GET_CAPABILITIES = 2,
    MNN_RPC_CMD_PING = 3,
    MNN_RPC_CMD_REGISTER_GRAPH = 10,
    MNN_RPC_CMD_FREE_GRAPH = 11,
    MNN_RPC_CMD_RESIZE_GRAPH = 12,
    MNN_RPC_CMD_RECOMPUTE_GRAPH = 13,
    MNN_RPC_CMD_PUSH_WEIGHT = 20,
    MNN_RPC_CMD_PUSH_INPUT = 21,
    MNN_RPC_CMD_PULL_OUTPUT = 22,
    MNN_RPC_CMD_PUSH_KV = 23,
    MNN_RPC_CMD_PULL_KV = 24,
    MNN_RPC_CMD_RUN_SEGMENT = 30,
    MNN_RPC_CMD_RUN_SEGMENT_ASYNC = 31,
    MNN_RPC_CMD_WAIT = 32,
    MNN_RPC_CMD_CANCEL = 33,
    MNN_RPC_CMD_GC = 40,
    MNN_RPC_CMD_GET_STATS = 41,
} MNNRPCCommand;

typedef enum {
    MNN_RPC_STATUS_OK = 0,
    MNN_RPC_STATUS_BAD_REQUEST = 1,
    MNN_RPC_STATUS_UNSUPPORTED = 2,
    MNN_RPC_STATUS_IO_ERROR = 3,
    MNN_RPC_STATUS_INTERNAL_ERROR = 4,
} MNNRPCStatus;

typedef enum {
    MNN_RPC_REMOTE_F_HOST_OWNS_WEIGHTS = 1u << 0,
    MNN_RPC_REMOTE_F_ENABLE_WEIGHT_CACHE = 1u << 1,
    MNN_RPC_REMOTE_F_ENABLE_GRAPH_CACHE = 1u << 2,
    MNN_RPC_REMOTE_F_ENABLE_ASYNC = 1u << 3,
} MNNRPCRemoteFlags;

typedef struct {
    const char* host;
    uint16_t port;
    MNNForwardType backendType;
    int32_t deviceId;
    float weight;
    const char* cachePath;
} MNNRPCNodeConfig;

typedef struct {
    const char* name;
    int32_t opStart;
    int32_t opEnd;
    int32_t layerStart;
    int32_t layerEnd;
    uint32_t nodeIndex;
    uint32_t flags;
} MNNRPCSegmentConfig;

typedef struct {
    uint32_t structSize;
    uint32_t configVersion;
    uint32_t protocolVersion;
    uint32_t flags;
    uint32_t connectTimeoutMs;
    uint32_t ioTimeoutMs;
    uint32_t maxPacketSize;
    const char* authToken;
    size_t nodeCount;
    const MNNRPCNodeConfig* nodes;
    size_t segmentCount;
    const MNNRPCSegmentConfig* segments;
} MNNRPCClientConfig;

typedef struct {
    MNNForwardType backendType;
    int32_t deviceId;
    int32_t memoryMB;
    int32_t computeUnits;
    uint32_t flags;
} MNNRPCDeviceCapability;

#ifdef __cplusplus
}
#endif

#endif // MNN_RPC_H
