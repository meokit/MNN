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
    if (op == nullptr || op->type() != OpType_Extra || op->main_type() != OpParameter_Extra) {
        return nullptr;
    }
    auto extra = op->main_as_Extra();
    if (extra == nullptr || extra->type() == nullptr || extra->type()->str() != "RPCSegment") {
        return nullptr;
    }
    return new RemoteExecution(this, mClient);
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

RemoteExecution::RemoteExecution(Backend* backend, std::shared_ptr<Client> client) : Execution(backend), mClient(std::move(client)) {
}

ErrorCode RemoteExecution::onResize(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs) {
    (void)inputs;
    (void)outputs;
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
