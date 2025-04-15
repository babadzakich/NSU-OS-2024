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

 void Cache::put(const string& path, const string& head, const string& body, size_t size, bool has_size) {
    CacheEntry entry = CacheEntry(head, body, size, time(nullptr), DEFAULT_TTL, size == body.size(), has_size);
    cerr << "Cache size: " << size << endl;
    cerr << "Cache head size: " << head.size() << endl;
    cerr << "Cache body size: " << body.size() << endl;
    cerr << "Uploaded: " << entry.uploaded << endl;
    cache_storage[path] = entry;
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


bool Cache::append(const string& path, const string& data) {
    auto it = cache_storage.find(path);
    if (it == cache_storage.end()) {
        return false;
    }
    it->second.body += data;
    return true;
}

void Cache::set_uploaded(const string& path, bool uploaded) {
    auto it = cache_storage.find(path);
    if (it != cache_storage.end()) {
        it->second.uploaded = uploaded;
    }
}
