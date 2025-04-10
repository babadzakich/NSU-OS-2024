#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <unordered_map>
#include <ctime>

struct CacheEntry {
    std::string response;
    time_t timestamp;
    int ttl;
};

class Cache {
private:
    std::unordered_map<std::string, CacheEntry> cache_storage;
    const int DEFAULT_TTL = 300;

public:
    bool get(const std::string& key, std::string& response);
    void put(const std::string& key, const std::string& response, int ttl = -1);
    void cleanup();
};

#endif
