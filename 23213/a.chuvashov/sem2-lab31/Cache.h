#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <unordered_map>
#include <ctime>
#include <iostream>

using namespace std;

struct CacheEntry {
    string response;
    size_t size;
    time_t timestamp;
    int ttl;
    bool uploaded;
};

class Cache {
private:
    static unordered_map<string, CacheEntry> cache_storage;
    static const int DEFAULT_TTL = 300;
    static string cache_key(const string& method, const string& path);
public:
    static const size_t MAX_CACHEABLE_SIZE = 50 * 1024 * 1024;

    static bool append(const string& method, const string& path, const string& data);
    static bool get(const string& method, const string& path, CacheEntry& response);
    static void put(const string& method, const string& path, const string& response, size_t size, int ttl = DEFAULT_TTL);
    static void delete_entry(const string& method, const string& path);
    static bool is_uploaded(const string& method, const string& path);
    static void cleanup();
};

#endif
