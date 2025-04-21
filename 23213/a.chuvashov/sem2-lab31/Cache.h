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

    CacheEntry() : head(""), body(""), body_size(0), timestamp(time(nullptr)), ttl(300), uploaded(false), has_size(false) {}
};

class Cache {
private:
    static unordered_map<string, CacheEntry> cache_storage;
    static const int DEFAULT_TTL = 300;
public:
    static const size_t MAX_CACHEABLE_SIZE = 50 * 1024 * 1024;

    static bool append_head(const string& path, const string& data);
    static bool append_body(const string& path, const string& data);
    static void set_uploaded(const string& path, bool uploaded);
    static void set_size(const string& path, size_t size);
    static void make_entry(const string& path);
    static bool get(const string& path, CacheEntry& response);
    static void delete_entry(const string& path);
    static bool is_uploaded(const string& path);
    static void cleanup();
};

#endif
