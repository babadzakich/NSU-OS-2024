#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <unordered_map>
#include <ctime>
#include <iostream>
#include <memory>


using namespace std;

struct CacheEntry {
    string head;
    string body;
    size_t body_size;
    time_t timestamp;
    int ttl;
    bool uploaded;
    bool has_size;

    CacheEntry() : head(""), body(""), body_size(0), timestamp(0), ttl(0), uploaded(false), has_size(false) {}
    CacheEntry(const string& h, const string& b, size_t s, time_t t, int ttl_val, bool u, bool c)
        : head(h), body(b), body_size(s), timestamp(t), ttl(ttl_val), uploaded(u), has_size(c) {}
};

class Cache {
private:
    static unordered_map<string, CacheEntry> cache_storage;
    static const int DEFAULT_TTL = 300;
public:
    static const size_t MAX_CACHEABLE_SIZE = 50 * 1024 * 1024;

    static bool append(const string& path, const string& data);
    static void set_uploaded(const string& path, bool uploaded);
    static bool get(const string& path, CacheEntry& response);
    static void put(const string& path, const string& head, const string& body, size_t size, bool has_size);
    static void delete_entry(const string& path);
    static bool is_uploaded(const string& path);
    static void cleanup();
};

#endif
