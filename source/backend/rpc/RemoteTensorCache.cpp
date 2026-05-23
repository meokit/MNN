//
//  RemoteTensorCache.cpp
//  MNN
//

#include "RemoteTensorCache.hpp"

namespace MNN {
namespace RPC {

bool RemoteTensorCache::insertOrRefresh(const TensorCacheEntry& entry, TensorCacheEntry* stored) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mEntries.find(entry.tensorUid);
    bool inserted = iter == mEntries.end();
    if (inserted) {
        auto value = entry;
        value.pushCount = 1;
        iter = mEntries.insert(std::make_pair(entry.tensorUid, value)).first;
    } else {
        iter->second.byteSize = entry.byteSize;
        iter->second.dataType = entry.dataType;
        iter->second.dimensions = entry.dimensions;
        iter->second.flags = entry.flags;
        iter->second.data = entry.data;
        iter->second.pushCount += 1;
    }
    if (stored != nullptr) {
        *stored = iter->second;
    }
    return inserted;
}

bool RemoteTensorCache::find(uint64_t tensorUid, TensorCacheEntry* entry) const {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mEntries.find(tensorUid);
    if (iter == mEntries.end()) {
        return false;
    }
    if (entry != nullptr) {
        *entry = iter->second;
    }
    return true;
}

bool RemoteTensorCache::erase(uint64_t tensorUid, TensorCacheEntry* removed) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mEntries.find(tensorUid);
    if (iter == mEntries.end()) {
        return false;
    }
    if (removed != nullptr) {
        *removed = iter->second;
    }
    mEntries.erase(iter);
    return true;
}

void RemoteTensorCache::clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mEntries.clear();
}

size_t RemoteTensorCache::size() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mEntries.size();
}

} // namespace RPC
} // namespace MNN
