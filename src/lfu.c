#include "tinycache.h"

cache_entry* find_lfu_victim(Cache* c) {
    cache_entry* victim = NULL;

    // traverse through LRU list (LRU to MRU)
    for(cache_entry* ce = c->lru_tail; ce != NULL; ce = ce->lru_prev) {
        if(victim == NULL || ce->frequency < victim->frequency) {
            victim = ce;
        }
        // uses LRU as tie-breaker when more than one entry has same access frequency
    }   

    return victim;
}