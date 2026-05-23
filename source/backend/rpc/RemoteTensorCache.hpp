//
//  RemoteTensorCache.hpp
//  MNN
//

#ifndef MNN_SOURCE_BACKEND_RPC_REMOTETENSORCACHE_HPP
#define MNN_SOURCE_BACKEND_RPC_REMOTETENSORCACHE_HPP

#include <stddef.h>
#include <map>
#include <mutex>
#include <stdint.h>
#include <vector>

namespace MNN {
namespace RPC {

struct TensorCacheEntry {
    uint64_t tensorUid = 0;
    uint64_t byteSize = 0;
    uint32_t dataType = 0;
    uint32_t dimensions = 0;
    uint32_t flags = 0;
    uint32_t pushCount = 0;
    std::vector<uint8_t> data;
};

class RemoteTensorCache {
public:
    bool insertOrRefresh(const TensorCacheEntry& entry, TensorCacheEntry* stored = nullptr);
    bool find(uint64_t tensorUid, TensorCacheEntry* entry = nullptr) const;
    bool erase(uint64_t tensorUid, TensorCacheEntry* removed = nullptr);
    void clear();
    size_t size() const;

private:
    mutable std::mutex mMutex;
    std::map<uint64_t, TensorCacheEntry> mEntries;
};

} // namespace RPC
} // namespace MNN

#endif
