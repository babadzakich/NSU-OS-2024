#include "Cache.h"

unordered_map<string, CacheEntry> Cache::cache_storage;

bool Cache::get(const string& path, CacheEntry& response) {
    auto it = cache_storage.find(path);
    if (it == cache_storage.end()) {
        return false;
    }
    time_t now = time(nullptr);
    if (now - it->second.timestamp > it->second.ttl) {
        cache_storage.erase(it);
        return false;
    }

    response = it->second;
    return true;
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

bool Cache::is_uploaded(const string& path) {
    auto it = cache_storage.find(path);
    if (it == cache_storage.end()) {
        return false;
    }
    return it->second.uploaded;
}

void Cache::delete_entry(const string& path) {
    cache_storage.erase(path);
}


bool Cache::append_body(const string& path, const string& data) {
    auto it = cache_storage.find(path);
    if (it == cache_storage.end()) {
        return false;
    }
    it->second.body += data;
    return true;
}

bool Cache::append_head(const string& path, const string& data) {
    auto it = cache_storage.find(path);
    if (it == cache_storage.end()) {
        return false;
    }
    it->second.head += data;
    return true;
}

void Cache::set_uploaded(const string& path, bool uploaded) {
    auto it = cache_storage.find(path);
    if (it != cache_storage.end()) {
        it->second.uploaded = uploaded;
    }
}

void Cache::make_entry(const string& path) {
    if (cache_storage.find(path) != cache_storage.end()) {
        return;
    }
    cache_storage[path] = CacheEntry();
}

void Cache::set_size(const string& path, size_t size) {
    auto it = cache_storage.find(path);
    if (it != cache_storage.end()) {
        it->second.body_size = size;
        it->second.has_size = true;
    }
}
