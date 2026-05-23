//
//  RemoteGraphCache.cpp
//  MNN
//

#include "RemoteGraphCache.hpp"

namespace MNN {
namespace RPC {

bool RemoteGraphCache::insertOrRefresh(const GraphCacheEntry& entry, GraphCacheEntry* stored) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mEntries.find(entry.graphUid);
    bool inserted = iter == mEntries.end();
    if (inserted) {
        auto value = entry;
        value.registerCount = 1;
        iter = mEntries.insert(std::make_pair(entry.graphUid, value)).first;
    } else {
        iter->second.segmentUid = entry.segmentUid;
        iter->second.shapeSignature = entry.shapeSignature;
        iter->second.opCount = entry.opCount;
        iter->second.tensorCount = entry.tensorCount;
        iter->second.flags = entry.flags;
        iter->second.registerCount += 1;
    }
    if (stored != nullptr) {
        *stored = iter->second;
    }
    return inserted;
}

bool RemoteGraphCache::erase(uint64_t graphUid, GraphCacheEntry* removed) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mEntries.find(graphUid);
    if (iter == mEntries.end()) {
        return false;
    }
    if (removed != nullptr) {
        *removed = iter->second;
    }
    mEntries.erase(iter);
    return true;
}

bool RemoteGraphCache::find(uint64_t graphUid, GraphCacheEntry* entry) const {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mEntries.find(graphUid);
    if (iter == mEntries.end()) {
        return false;
    }
    if (entry != nullptr) {
        *entry = iter->second;
    }
    return true;
}

void RemoteGraphCache::clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mEntries.clear();
}

size_t RemoteGraphCache::size() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mEntries.size();
}

} // namespace RPC
} // namespace MNN
