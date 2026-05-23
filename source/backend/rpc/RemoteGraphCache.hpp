//
//  RemoteGraphCache.hpp
//  MNN
//

#ifndef MNN_SOURCE_BACKEND_RPC_REMOTEGRAPHCACHE_HPP
#define MNN_SOURCE_BACKEND_RPC_REMOTEGRAPHCACHE_HPP

#include <stddef.h>
#include <map>
#include <mutex>
#include <stdint.h>

namespace MNN {
namespace RPC {

struct GraphCacheEntry {
    uint64_t graphUid = 0;
    uint64_t segmentUid = 0;
    uint64_t shapeSignature = 0;
    uint32_t opCount = 0;
    uint32_t tensorCount = 0;
    uint32_t flags = 0;
    uint32_t registerCount = 0;
};

class RemoteGraphCache {
public:
    bool insertOrRefresh(const GraphCacheEntry& entry, GraphCacheEntry* stored = nullptr);
    bool erase(uint64_t graphUid, GraphCacheEntry* removed = nullptr);
    bool find(uint64_t graphUid, GraphCacheEntry* entry = nullptr) const;
    void clear();
    size_t size() const;

private:
    mutable std::mutex mMutex;
    std::map<uint64_t, GraphCacheEntry> mEntries;
};

} // namespace RPC
} // namespace MNN

#endif
