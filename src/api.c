// cache api

#include "tinycache.h"

void hkv_set(Cache* c, Request* req, Response* res) {
    
    if (c == NULL || req == NULL ||
        req->key == NULL ||
        (req->value == NULL && req->valuelen > 0)) {
        
        res->status = HKV_STATUS_DENIED;
        res->msglen  = 18;
        res->msg = "Malformed request";
        return;
    }
    
    bool ret = false;
    
    // first add entry on disk
    ret = tc_add_record(c, req->key, req->value, req->valuelen);
    if(!ret) {
        res->status = HKV_STATUS_INTERR;
        res->msglen  = 28;
        res->msg = "Failed during write on disk";
        return;
    }

    // then add entry in cache
    size_t bucket_idx = sdbm_hash(req->key) % TC_BUCKET_COUNT;
    ret = insert(
        c,
        bucket_idx,
        req->key,
        req->value, req->valuelen,
        req->ttl    // sets HKV_NO_EXPIRY by caller
    );

    if(!ret) {
        res->status = HKV_STATUS_INTERR;
        res->msglen  = 30;
        res->msg = "Failed during write in-memory";
        return;
    } 
    else {
        res->status = HKV_STATUS_OK;
        res->msglen  = 8;
        res->msg = "Success";
    }
}

void hkv_setex(Cache* c, Request* req, Response* res) {

    if (c == NULL || req == NULL ||
        req->key == NULL ||
        (req->value == NULL && req->valuelen > 0)) {
        
        res->status = HKV_STATUS_DENIED;
        res->msglen  = 18;
        res->msg = "Malformed request";
        return;
    }
    
    bool ret = false;
    
    //  add entry only in cache
    size_t bucket_idx = sdbm_hash(req->key) % TC_BUCKET_COUNT;
    ret = insert(
        c,
        bucket_idx,
        req->key,
        req->value, req->valuelen,
        req->ttl  
    );

    if(!ret) {
        res->status = HKV_STATUS_INTERR;
        res->msglen  = 30;
        res->msg = "Failed during write in-memory";
        return;
    } 
    else {
        res->status = HKV_STATUS_OK;
        res->msglen  = 8;
        res->msg = "Success";
    }
}

void hkv_get(Cache* c, Request* req, Response* res) {
    
    size_t bucket_idx = sdbm_hash(req->key) % TC_BUCKET_COUNT;
    pthread_mutex_lock(&c->lock[bucket_idx]);

    cache_entry* ce = search(c, req->key, bucket_idx);

    // cache hit
    if(ce != NULL) {
        
        // lazy expiration check for ephemeral entries
        if(ce->ttl != HKV_NO_EXPIRY && ce->ttl < (int32_t)time(NULL)) {
            delete(c, ce->key, bucket_idx);
            pthread_mutex_unlock(&c->lock[bucket_idx]);

            res->status = HKV_STATUS_NFOUND;
            res->msg = "Requested key not found";
            res->msglen = 24;
            res->value = NULL;
            res->valuelen = 0;
            return;
        }

        uint32_t value_len = (uint32_t)ce->value_len;

        // value is copied instead of borrowing pointer to avoid collision with concurrent access
        void* value = malloc(sizeof(ce->value_len));
        if (ce->value_len > 0 && value == NULL) {
            pthread_mutex_unlock(&c->lock[bucket_idx]);
            
            res->status = HKV_STATUS_INTERR;
            res->msg = "Failed allocation entry from cache";
            res->msglen = (uint8_t)strlen(res->msg);
            return;
        }
        memcpy(value, ce->value, ce->value_len);

        // update LRU
        pthread_mutex_lock(&c->eviction_lock);
        lru_update_lock(c, ce);
        pthread_mutex_unlock(&c->eviction_lock);
        
        pthread_mutex_unlock(&c->lock[bucket_idx]);
        
        res->status = HKV_STATUS_OK;
        res->msglen = 8;
        res->msg = "Success";
        res->valuelen = value_len;
        res->value = value;
        return;
    }

    pthread_mutex_unlock(&c->lock[bucket_idx]);

    // Cache miss: retrieve from persistent storage

    // does not exist on disk record either
    if(!tc_get_record(c, req->key, &res->value, res->valuelen)) {
        res->status = HKV_STATUS_NFOUND;
        res->msglen = 24;
        res->msg = "Requested key not found";
        return;
    }

    tc_log(TC_DEBUG, "Found in disk (Key: %s | Value: %s)", req->key, (char*)res->value);
    tc_log(TC_DEBUG, "Key: %s Adding to cache (cache miss)", req->key);

    /*
        Populate cache after cache miss
        Only persistent entries has this case which has no expiry
    */
    insert(c, bucket_idx, req->key, res->value, res->valuelen, HKV_NO_EXPIRY);
    
    res->status = HKV_STATUS_OK;
    res->msglen = 8;
    res->msg = "Success";
}

void hkv_delete(Cache* c, Request* req, Response* res) {

    size_t bucket_idx = sdbm_hash(req->key) % TC_BUCKET_COUNT;    

    bool ret;
    // first remove entry from cache
    pthread_mutex_lock(&c->lock[bucket_idx]);
    ret = delete(c, req->key, bucket_idx);
    pthread_mutex_unlock(&c->lock[bucket_idx]);
    
    if(!ret) {
        res->status = HKV_STATUS_INTERR;
        res->msglen = 32;
        res->msg = "Failed to delete key from cache";
        return;
    } 
    
    // then remove entry from disk
    ret = tc_delete_record(c, req->key);
    if(!ret) {
        res->status = HKV_STATUS_INTERR;
        res->msglen = 31;
        res->msg = "Failed to delete key from disk";
        return;
    }
        
    res->status =  HKV_STATUS_OK;
    res->msglen = 8;
    res->msg = "Success";
}

void hkv_exists(Cache* c, Request* req, Response* res) {
    
    size_t bucket_idx = sdbm_hash(req->key) % TC_BUCKET_COUNT;
    pthread_mutex_lock(&c->lock[bucket_idx]);

    cache_entry* ce = search(c, req->key, bucket_idx);
    if(ce == NULL) {
        tc_log(TC_DEBUG, "Key not found: Already deleted");
        pthread_mutex_unlock(&c->lock[bucket_idx]);
        
        res->status = HKV_STATUS_NFOUND;
        res->msglen = 24;
        res->msg = "Requested key not found";
        return;
    }

    // lazy expiration check for ephemeral entries
    if(ce->ttl != HKV_NO_EXPIRY && ce->ttl < (int32_t)time(NULL)) {
        delete(c, ce->key, bucket_idx);
        pthread_mutex_unlock(&c->lock[bucket_idx]);

        res->status = HKV_STATUS_NFOUND;
        res->msg = "Requested key not found";
        res->msglen = 24;
        res->value = NULL;
        res->valuelen = 0;
        return;
    }

    // update LRU
    pthread_mutex_lock(&c->eviction_lock);
    lru_update_lock(c, ce);
    pthread_mutex_unlock(&c->eviction_lock);

    pthread_mutex_unlock(&c->lock[bucket_idx]);

    res->status = HKV_STATUS_NFOUND;
    res->msglen = 24;
    res->msg = "Requested key not found";
}

void hkv_evict(Cache* c, Request* req, Response* res) {

    size_t bucket_idx = sdbm_hash(req->key) % TC_BUCKET_COUNT;    
    bool ret;

    pthread_mutex_lock(&c->lock[bucket_idx]);

    cache_entry* ce = search(c, req->key, bucket_idx);
    if(ce == NULL) {
        pthread_mutex_unlock(&c->lock[bucket_idx]);
        res->status = HKV_STATUS_NFOUND;
        res->msglen = 24;
        res->msg = "Requested key not found";
        return;
    }

    // Eviction only allowed for persistent entries
    if(ce->ttl != HKV_NO_EXPIRY) {
        pthread_mutex_unlock(&c->lock[bucket_idx]);
        res->status = HKV_STATUS_DENIED;
        res->msglen = 53;
        res->msg = "Explicit eviction not allowed for ephemeral entriess";
        return;
    }

    ret = delete(c, req->key, bucket_idx); 
    pthread_mutex_unlock(&c->lock[bucket_idx]);

    if(!ret) {
        res->status = HKV_STATUS_INTERR;
        res->msglen = 31;
        res->msg = "Failed to evict key from cache";
        return;
    } 

    res->status = HKV_STATUS_NFOUND;
    res->msglen = 8;
    res->msg = "Success";
}

// sets TTL to 0
void hkv_expire(Cache* c, Request* req, Response* res) {

    size_t bucket_idx = sdbm_hash(req->key) % TC_BUCKET_COUNT;
    cache_entry* ce = search(c, req->key, bucket_idx);
    
    bool ret;

    // first check whether key is ephemeral for lazy expiration 
    if(ce->ttl == HKV_NO_EXPIRY) {
        res->status = HKV_STATUS_DENIED;
        res->msg = "Persistent entries can not be expired";
        res->msglen = 38;
        res->value = NULL;
        res->valuelen = 0;
        return;
    }

    // second check for lazy expiration
    if(ce->ttl < (int32_t)time(NULL)) {
        
        pthread_mutex_lock(&c->lock[bucket_idx]);
        delete(c, ce->key, bucket_idx);
        pthread_mutex_unlock(&c->lock[bucket_idx]);

        res->status = HKV_STATUS_NFOUND;
        res->msg = "Requested key not found";
        res->msglen = 24;
        res->value = NULL;
        res->valuelen = 0;
        return;
    }

    ret = update(c, ce, bucket_idx, ce->value, ce->value_len, HKV_IMMEDIATE_EXPIRY);

    if(!ret) {
        res->status = HKV_STATUS_INTERR;
        res->msg = "Failed to expire key";
        res->msglen = 21;
        res->value = NULL;
        res->valuelen = 0;
        return;
    }

    res->status = HKV_STATUS_OK;
    res->msg = "Success";
    res->msglen = 8;
    res->value = NULL;
    res->valuelen = 0;

    return;
}

void hkv_ttl(Cache* c, Request* req, Response* res) {

    size_t bucket_idx = sdbm_hash(req->key) % TC_BUCKET_COUNT;
    cache_entry* ce = search(c, req->key, bucket_idx);
    
    bool ret;

    // first check whether key is ephemeral for lazy expiration 
    if(ce->ttl == HKV_NO_EXPIRY) {
        res->status = HKV_STATUS_DENIED;
        res->msg = "Persistent entries can not have TTL";
        res->msglen = 36;
        res->value = NULL;
        res->valuelen = 0;
        return;
    }

    // second check for lazy expiration
    if(ce->ttl < (int32_t)time(NULL)) {
        
        pthread_mutex_lock(&c->lock[bucket_idx]);
        delete(c, ce->key, bucket_idx);
        pthread_mutex_unlock(&c->lock[bucket_idx]);

        res->status = HKV_STATUS_NFOUND;
        res->msg = "Requested key not found";
        res->msglen = 24;
        res->value = NULL;
        res->valuelen = 0;
        return;
    }

    int64_t new_ttl =  req->ttl + (ce->ttl - time(NULL));
    ret = update(c, ce, bucket_idx, ce->value, ce->value_len, new_ttl);

    if(!ret) {
        res->status = HKV_STATUS_INTERR;
        res->msg = "Failed to update TTL";
        res->msglen = 21;
        res->value = NULL;
        res->valuelen = 0;
        return;
    }

    res->status = HKV_STATUS_OK;
    res->msg = "Success";
    res->msglen = 8;
    res->value = NULL;
    res->valuelen = 0;

    return;
}

void hkv_list(Cache* c, Request* req, Response* res) {

}

void hkv_stat(Cache* c, Request* req, Response* res) {
    
    char response[16];

    if(memcmp(req->key, "MEM", 4) == 0) {
        size_t cache_mem_bytes = atomic_load_explicit(
            &c->mem_used, memory_order_relaxed
        );
        int len = snprintf(response, sizeof(response), "%zu", cache_mem_bytes);

        res->status = HKV_STATUS_OK;
        res->msglen = 8;
        res->msg = "Success";
        res->valuelen = (uint32_t)len;
        res->value = response;
        return;
    }

    if(memcmp(req->key, "KEY", 4) == 0) {
        size_t cache_key_count = c->entries;
        int len = snprintf(response, sizeof(response), "%zu", cache_key_count);

        res->status = HKV_STATUS_OK;
        res->msglen = 8;
        res->msg = "Success";
        res->valuelen = (uint32_t)len;
        res->value = response;
        return;
    }

    res->status = HKV_STATUS_INVREQ;
    res->msglen = 23;
    res->msg = "Invalid status request";
}
