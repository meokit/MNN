//
//  RemoteBackend.cpp
//  MNN
//

#include "RemoteBackend.hpp"
#include <stdlib.h>
#include <string.h>
#include "MNN_generated.h"
#include "core/TensorUtils.hpp"

namespace MNN {
namespace RPC {

namespace {
static uint64_t hashString(const char* content) {
    const uint64_t offset = 1469598103934665603ull;
    const uint64_t prime = 1099511628211ull;
    uint64_t value = offset;
    if (content == nullptr) {
        return value;
    }
    while (*content) {
        value ^= static_cast<uint8_t>(*content);
        value *= prime;
        ++content;
    }
    return value;
}
}

RemoteBackend::RemoteMemObj::RemoteMemObj(size_t size) {
    if (size > 0) {
        mPtr = MNNMemoryAllocAlign(size, MNN_MEMORY_ALIGN_DEFAULT);
    }
}

RemoteBackend::RemoteMemObj::~RemoteMemObj() {
    if (mPtr != nullptr) {
        MNNMemoryFreeAlign(mPtr);
        mPtr = nullptr;
    }
}

MemChunk RemoteBackend::RemoteMemObj::chunk() {
    return MemChunk(mPtr, 0);
}

RemoteBackend::RemoteBackend(std::shared_ptr<Client> client) : Backend(MNN_FORWARD_USER_2), mClient(std::move(client)) {
}

Execution* RemoteBackend::onCreate(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs, const MNN::Op* op) {
    (void)inputs;
    (void)outputs;
    if (op == nullptr || op->type() != OpType_Extra || op->main_type() != OpParameter_Extra) {
        return nullptr;
    }
    auto extra = op->main_as_Extra();
    if (extra == nullptr || extra->type() == nullptr || extra->type()->str() != "RPCSegment") {
        return nullptr;
    }
    GraphDescriptor graph;
    graph.graphUid = hashString(op->name() == nullptr ? nullptr : op->name()->c_str());
    graph.segmentUid = hashString(extra->engine() == nullptr ? nullptr : extra->engine()->c_str());
    graph.shapeSignature = graph.graphUid ^ (graph.segmentUid << 1);
    graph.opCount = 1;
    return new RemoteExecution(this, mClient, graph);
}

ErrorCode RemoteBackend::onResizeEnd() {
    return NO_ERROR;
}

void RemoteBackend::onExecuteBegin() const {
}

void RemoteBackend::onExecuteEnd() const {
}

Backend::MemObj* RemoteBackend::onAcquire(const Tensor* tensor, StorageType storageType) {
    (void)storageType;
    auto size = tensor == nullptr ? 0 : static_cast<size_t>(tensor->size()) * static_cast<size_t>(tensor->getType().bytes());
    return new RemoteMemObj(size);
}

bool RemoteBackend::onClearBuffer() {
    return true;
}

void RemoteBackend::onCopyBuffer(const Tensor* srcTensor, const Tensor* dstTensor) const {
    MNNCPUCopyBuffer(srcTensor, dstTensor);
}

const Runtime* RemoteBackend::getRuntime() {
    return mRuntime;
}

std::shared_ptr<Client> RemoteBackend::client() const {
    return mClient;
}

void RemoteBackend::setRuntime(const Runtime* runtime) {
    mRuntime = runtime;
}

void RemoteBackend::setLastError(const std::string& error) {
    mLastError = error;
}

const std::string& RemoteBackend::lastError() const {
    return mLastError;
}

RemoteExecution::RemoteExecution(Backend* backend, std::shared_ptr<Client> client, const GraphDescriptor& graph)
    : Execution(backend), mClient(std::move(client)), mGraph(graph) {
}

ErrorCode RemoteExecution::onResize(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs) {
    mGraph.tensorCount = static_cast<uint32_t>(inputs.size() + outputs.size());
    if (mRegistered || mClient == nullptr) {
        return NO_ERROR;
    }
    if (!mClient->registerGraph(mGraph)) {
        auto remote = static_cast<RemoteBackend*>(backend());
        remote->setLastError(mClient->lastError());
        return NOT_SUPPORT;
    }
    mRegistered = true;
    return NO_ERROR;
}

ErrorCode RemoteExecution::onExecute(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs) {
    (void)inputs;
    (void)outputs;
    if (!mClient || !mClient->ping()) {
        auto remote = static_cast<RemoteBackend*>(backend());
        remote->setLastError(mClient ? mClient->lastError() : "RPC client is null");
        return NOT_SUPPORT;
    }
    return NO_ERROR;
}

} // namespace RPC
} // namespace MNN
