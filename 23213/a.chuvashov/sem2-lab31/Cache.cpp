#include "Cache.h"

unordered_map<string, CacheEntry> Cache::cache_storage;

bool Cache::get(const string& method, const string& path, CacheEntry& response) {
    auto it = cache_storage.find(cache_key(method, path));
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

 void Cache::put(const string& method, const string& path, const string& response, size_t size, int ttl) {
    CacheEntry entry;
    entry.response = response;
    entry.timestamp = time(nullptr);
    entry.ttl = (ttl > 0) ? ttl : DEFAULT_TTL;
    entry.size = size;
    entry.uploaded = (size == response.size());
    cerr << "Cache size: " << size;
    cerr << " Cache response size: " << response.size() << endl;
    cerr << "Uploaded: " << entry.uploaded << endl;
    string key = cache_key(method, path);
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

bool Cache::is_uploaded(const string& method, const string& path) {
    auto it = cache_storage.find(cache_key(method, path));
    if (it == cache_storage.end()) {
        return false;
    }
    return it->second.uploaded;
}

void Cache::delete_entry(const string& method, const string& path) {
    cache_storage.erase(cache_key(method, path));
}

string Cache::cache_key(const string& method, const string& path) {
    return method + " " + path;
}


bool Cache::append(const string& method, const string& path, const string& data) {
    auto it = cache_storage.find(cache_key(method, path));
    if (it == cache_storage.end()) {
        return false;
    }
    it->second.response += data;
    return true;
}