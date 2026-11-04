#include "tinycache.h"

/*
    Caller is responsible to hold cache lock
    Adds new entry to the head of LRU list
    Remember to lock cache before locking eviction 
    And unlock eviction before unlocking cache
*/
void lru_insert_lock(Cache* c, cache_entry* ce) {
    ce->lru_prev = NULL;
    ce->lru_next = c->lru_head;

    if(c->lru_head != NULL) {
        c->lru_head->lru_prev = ce;
    }
    else {
        // add in tail when LRU list is empty
        c->lru_tail = ce;
    }

    c->lru_head = ce;
}

/*
    Caller is responsible to hold cache lock
    Removes entry from the tail of LRU list during DELETE
    Remember to lock cache before locking eviction 
    And unlock eviction before unlocking cache
*/
void lru_remove_lock(Cache* c, cache_entry* ce) {

    if(ce->lru_prev != NULL) {
        // remove ownself from LRU list
        ce->lru_prev->lru_next = ce->lru_next;
    } 
    else {
        // when first entry of LRU
        c->lru_head = ce->lru_next;
    }

    if(ce->lru_next != NULL) {
        // remove ownself from LRU list
        ce->lru_next->lru_prev = ce->lru_prev;
    } 
    else {
        // when last entry of LRU
        c->lru_tail = ce->lru_prev;
    }

    ce->lru_prev = NULL;
    ce->lru_next = NULL;
}

/*
    Caller is responsible to hold cache lock
    Moves entry to the head of LRU list during GET, SET (update), EXISTS operations
    Remember to lock cache before locking eviction 
    And unlock eviction before unlocking cache
*/
void lru_update_lock(Cache* c, cache_entry* ce) {

    // remove from earlier position in LRU list
    lru_remove_lock(c, ce);
    // then move to the head of LRU list
    lru_insert_lock(c, ce);
}
