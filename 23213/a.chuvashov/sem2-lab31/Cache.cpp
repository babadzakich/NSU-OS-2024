#include "Cache.h"

bool Cache::get(const std::string& key, std::string& response) {
    auto it = cache_storage.find(key);
    if (it == cache_storage.end()) {
        return false;
    }

    time_t now = time(nullptr);
    if (now - it->second.timestamp > it->second.ttl) {
        cache_storage.erase(it);
        return false;
    }

    response = it->second.response;
    return true;
}

void Cache::put(const std::string& key, const std::string& response, int ttl) {
    CacheEntry entry;
    entry.response = response;
    entry.timestamp = time(nullptr);
    entry.ttl = (ttl > 0) ? ttl : DEFAULT_TTL;
    cache_storage[key] = entry;
}

void Cache::cleanup() {
    time_t now = time(nullptr);
    for (auto it = cache_storage.begin(); it != cache_storage.end();) {
        if (now - it->second.timestamp > it->second.ttl) {
            it = cache_storage.erase(it);
        } else {
            ++it;
        }
    }
}
