#include "tinycache.h"

// sdbm hash for KV cache and persistent record slot selection
uint64_t sdbm_hash(const char* key) {
    
    uint64_t hash = 0;
    int c;

    while ((c = *key++)) {
        // hash * 65599 + c
        hash = (uint64_t)c + (hash << 6) + (hash << 16) - hash; 
    }
    return hash;
}
