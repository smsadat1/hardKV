// cache api

#include "tinycache.h"

void hkv_set(Cache* c, Request* req, Response* res) {
    
    if (c == NULL || req == NULL ||
        req->key == NULL ||
        (req->value == NULL && req->valuelen > 0)) {
        return false;
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
        req->key,
        req->value, req->valuelen,
        bucket_idx
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
        
        void* value = malloc(ce->value_len);
        if (value == NULL && ce->value_len > 0) {
            pthread_mutex_unlock(&c->lock[bucket_idx]);
            return NULL;
        }

        if (ce->value_len > 0) {
            memcpy(res->value, ce->value, ce->value_len);
            res->valuelen = ce->value_len;
        }
        // update LRU
        pthread_mutex_lock(&c->eviction_lock);
        lru_update_lock(c, ce);
        pthread_mutex_unlock(&c->eviction_lock);

        pthread_mutex_unlock(&c->lock[bucket_idx]);

        res->status = HKV_STATUS_OK;
        res->msglen = 8;
        res->msg = "Success";
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

    // Populate cache after cache miss
    tc_log(TC_DEBUG, "Found in disk (Key: %s | Value: %s)", req->key, (char*)res->value);
    tc_log(TC_DEBUG, "Key: %s Adding to cache (cache miss)", req->key);
    insert(c, req->key, res->value, res->valuelen, bucket_idx);
    
    res->status = HKV_STATUS_OK;
    res->msglen = 8;
    res->msg = "Success";
}

void hkv_delete(Cache* c, Request* req, Response* res) {

    size_t bucket_idx = sdbm_hash(req->key) % TC_BUCKET_COUNT;    
    bool result = false;

    // first remove entry from cache
    pthread_mutex_lock(&c->lock[bucket_idx]);
    result = delete(c, req->key, bucket_idx);
    pthread_mutex_unlock(&c->lock[bucket_idx]);
    
    // then remove entry from disk
    if(!tc_delete_record(c, req->key)) {
        res->status = HKV_STATUS_INTERR;
        res->msglen = 21;
        res->msg = "Failed to delete key";
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
    bool result = false;
    pthread_mutex_lock(&c->lock[bucket_idx]);

    cache_entry* ce = search(c, req->key, bucket_idx);
    if(ce == NULL) {
        pthread_mutex_unlock(&c->lock[bucket_idx]);
        res->status = HKV_STATUS_NFOUND;
        res->msglen = 24;
        res->msg = "Requested key not found";
        return;
    }

    result = delete(c, req->key, bucket_idx); 
    pthread_mutex_unlock(&c->lock[bucket_idx]);

    res->status = HKV_STATUS_NFOUND;
    res->msglen = 8;
    res->msg = "Success";
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
        res->valuelen = len;
        res->value = response;
        return;
    }

    if(memcmp(req->key, "KEY", 4) == 0) {
        size_t cache_key_count = c->entries;
        int len = snprintf(response, sizeof(response), "%zu", cache_key_count);

        res->status = HKV_STATUS_OK;
        res->msglen = 8;
        res->msg = "Success";
        res->valuelen = len;
        res->value = response;
        return;
    }

    res->status = HKV_STATUS_INVREQ;
    res->msglen = 23;
    res->msg = "Invalid status request";
}
