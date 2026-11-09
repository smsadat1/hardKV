// cache implementations (hashmap)

#include "tinycache.h"

void init_cache(Cache* cache) {

    cache->buckets = (cache_entry**)calloc(TC_BUCKET_COUNT, sizeof(cache_entry*));
    if (cache->buckets == NULL) {
       perror("Failed to allocate cache buckets");
       abort();
    }

    cache->entries = 0;
    atomic_init(&cache->mem_used, 0);
    cache->mem_max = TC_MAX_MEMORY_MB * 1024ULL * 1024ULL;
    cache->lru_head = NULL;
    cache->lru_tail = NULL;

    if (pthread_mutex_init(&cache->eviction_lock, NULL) != 0)
        goto fail;

    atomic_init(&cache->shutdown, false);

    if (pthread_mutex_init(&cache->worker_lock, NULL) != 0)
        goto fail;

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);

    if (pthread_cond_init(&cache->worker_cond, &attr) != 0)
        goto fail;


    // initialize the pointer first
    cache->disk_lock = calloc(TC_MAX_RECORD_PARTITION, sizeof(*cache->disk_lock));
    if(cache->disk_lock == NULL) 
        goto fail;
        
    // either all locks initialize or none
    for(int i = 0; i < TC_MAX_RECORD_PARTITION; i++) {
        
        if (pthread_mutex_init(&cache->disk_lock[i], NULL) != 0) {
            while (i > 0)
                pthread_mutex_destroy(&cache->disk_lock[--i]);
            goto fail;
        }    
    }


    // initialize the pointer first
    cache->lock = calloc(TC_BUCKET_COUNT, sizeof(*cache->lock));
    if (cache->lock == NULL)
        goto fail;

    // either all locks initialize or none
    for(int i = 0; i < TC_BUCKET_COUNT; i++) {
        
        if (pthread_mutex_init(&cache->lock[i], NULL) != 0) {
            while (i > 0)
                pthread_mutex_destroy(&cache->lock[--i]);
            goto fail;
        }    
    }

    // return here, DO NOT slip to fail
    return;

fail:
    free(cache->buckets);
    cache->buckets = NULL;
    tc_log(TC_ERROR, "Resource initialization failed");
    abort();        

}

// Traverse and NULL whole cache before exit
void destroy_cache(Cache* cache) {
    
    if(cache == NULL)
        return;

    for(size_t i = 0; i < TC_BUCKET_COUNT; i++) {
        cache_entry* curr_ce = cache->buckets[i];

        while (curr_ce != NULL)
        {
            cache_entry* next_ce = curr_ce->next;
            free(curr_ce->key);
            free(curr_ce->value);
            free(curr_ce);
            curr_ce = next_ce;
        }
        cache->buckets[i] = NULL;
    } 

    pthread_cond_destroy(&cache->worker_cond);
    pthread_mutex_destroy(&cache->worker_lock);
    pthread_mutex_destroy(&cache->eviction_lock);

    for (int i = 0; i < TC_BUCKET_COUNT; i++)
        pthread_mutex_destroy(&cache->lock[i]);

    free(cache->lock);

    for(int i = 0; i < TC_MAX_RECORD_PARTITION; i++)
        pthread_mutex_destroy(&cache->disk_lock[i]);

    free(cache->disk_lock);

    atomic_init(&cache->mem_used, 0);
    cache->mem_max = 0;
    free(cache->buckets);
    cache->buckets = NULL;
}


bool update(
    Cache* c, cache_entry* ce, void* value, size_t value_len, size_t bucket_idx
) {
    void* new_value = NULL;
    
    if(value_len > 0) {
        new_value = malloc(value_len);
        if(new_value == NULL) {
            return false;
        }
        memcpy(new_value, value, value_len);
    }

    pthread_mutex_lock(&c->lock[bucket_idx]);

    /* update to new value length
        (later upate it with CAS loop)
    */
    atomic_fetch_sub(&c->mem_used, ce->value_len);
    atomic_fetch_add(&c->mem_used, value_len);

    // delete older value then add new value
    free(ce->value);
    ce->value = new_value;
    ce->value_len = (uint32_t)value_len;

    pthread_mutex_lock(&c->eviction_lock);
    lru_update_lock(c, ce);
    pthread_mutex_unlock(&c->eviction_lock);

    pthread_mutex_unlock(&c->lock[bucket_idx]);

    return true;
}

bool insert(
    Cache* c, char* key, void* value, size_t value_len, size_t bucket_idx
) {
    
    if (c == NULL || key == NULL ||
       (value == NULL && value_len > 0)) {
       return false;
    }

    cache_entry* ce = search(c, key, bucket_idx);
    if(ce != NULL) {
        // If key already exists, overwrite older value and return
        return update(c, ce, value, value_len, bucket_idx);
    }

    size_t bucket_index = sdbm_hash(key) % TC_BUCKET_COUNT;
    
    cache_entry* new_cache_entry = (cache_entry*)malloc(sizeof(cache_entry));
    if(new_cache_entry == NULL) {
        return false;
    }

    // assign key
    new_cache_entry->key = strdup(key);
    if(new_cache_entry->key == NULL) {
        free(new_cache_entry);
        return false;
    }

    // assign value
    new_cache_entry->value = NULL;
    if(value_len > 0) {
        new_cache_entry->value = malloc(value_len);

        if(new_cache_entry->value == NULL) {
            free(new_cache_entry->key);
            free(new_cache_entry);
            return false;
        }
        memcpy(new_cache_entry->value, value, value_len);
    }
    new_cache_entry->value_len = (uint32_t)value_len;

    // thread safety during entry
    pthread_mutex_lock(&c->lock[bucket_idx]);

    // add at the beginning of bucket
    new_cache_entry->next = c->buckets[bucket_index];
    c->buckets[bucket_index] = new_cache_entry;

    // increment entry & memory count
    c->entries += 1;   
    atomic_fetch_add(
        &c->mem_used, 
        new_cache_entry->value_len + strlen(new_cache_entry->key)
    );

    // update LRU
    pthread_mutex_lock(&c->eviction_lock);
    lru_insert_lock(c, new_cache_entry);
    pthread_mutex_unlock(&c->eviction_lock);

    pthread_mutex_unlock(&c->lock[bucket_idx]);

    return true;
}


cache_entry* search(Cache* c, char* key, size_t bucket_idx) {

    cache_entry* ce = c->buckets[bucket_idx];
    if(ce == NULL) {
        return NULL;
    }

    while (ce != NULL)
    {
        if(strcmp(key, ce->key) == 0) 
            return ce;
            
        ce = ce->next;
    }
    
    // should not reach here
    return NULL;
}

bool delete(Cache* c, char* key, size_t bucket_idx) {

    cache_entry* prev_entry = NULL;
    cache_entry* curr_entry = c->buckets[bucket_idx];
    if(curr_entry == NULL) {
        return false;
    }

    while (curr_entry != NULL)
    {
        if(strcmp(curr_entry->key, key) == 0) {

            // head node in bucket
            if(prev_entry == NULL) {
                c->buckets[bucket_idx] = curr_entry->next;
            }
            // last or middle node in bucket
            else {
                prev_entry->next = curr_entry->next;
            }

            // decrement count
            c->entries -= 1;
            atomic_fetch_sub(
                &c->mem_used,
                curr_entry->value_len + strlen(curr_entry->key)
            );

            // update LRU (remove entry from LRU list before final deletion)
            pthread_mutex_lock(&c->eviction_lock);
            lru_remove_lock(c, curr_entry);
            pthread_mutex_unlock(&c->eviction_lock);

            free(curr_entry->key);
            free(curr_entry->value);
            free(curr_entry);

            return true;
        }

        prev_entry = curr_entry;
        curr_entry = curr_entry->next;
    }
    
    return false;
}