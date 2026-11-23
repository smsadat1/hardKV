#include "tinycache.h"


void free_list(list* keylist) {
    if(keylist == NULL)
        return;

    for(size_t i = 0; i < keylist->count; i++)
    {
        free(keylist->items[i].key);
        free(keylist->items[i].value);
    }

    free(keylist->items);
    free(keylist);
}

/*
* Redis suggests to avoid KEYS style pattern matching because of single threaded nature
* But HardKV enforces per bucket locks in cache keeping mutex lock overhead substantially lower
* Thus KEYS <pattern> style is not a major issue for HardKV
*/
list* get_list(Cache* c, const char* pattern) {

    list* keylist = (list*)calloc(1, sizeof(*keylist));

    if (keylist == NULL) {
        tc_log(TC_ERROR, "Key list allocation failed");
        return NULL;
    }

    // Iterate through all buckets from beginning to end and find key matches
    for(int i = 0; i < TC_BUCKET_COUNT; i++) {

        // per bucket mutex lock
        pthread_mutex_lock(&c->lock[i]);
        
        cache_entry* ce = c->buckets[i];
        while (ce != NULL)
        {
            if(fnmatch(pattern, ce->key, 0) == 0) {
                
                // reuse keylist->items by moving allocation address
                entry* tmp = (entry*)realloc(keylist->items, keylist->count+1);
                if(tmp == NULL) {

                    tc_log(TC_ERROR, "Key result allocation failed");

                    pthread_mutex_unlock(&c->lock[i]);
                    
                    tc_free_keylist(keylist);
                    return NULL;
                }

                keylist->items = tmp;

                /*
                    Copy whole entry instead of borrowing pointers for safety
                    Because after lock release and before sending result over network
                    Different requests over cache might modify or free memory
                */
                entry* item = &keylist->items[keylist->count];
                item->key = strdup(ce->key);
                item->key_len = (uint16_t)strlen(ce->key);
                item->value = malloc(ce->value_len);
                
                if (item->key == NULL || (ce->value_len > 0 && item->value == NULL)) {
                    
                    tc_log(TC_ERROR, "Key list item allocation failed");

                    free(item->key);
                    free(item->value);

                    pthread_mutex_unlock(&c->lock[i]);

                    tc_free_keylist(keylist);
                    return NULL;
                }

                if(ce->value_len > 0)
                    memcpy(item->value, ce->value, ce->value_len);

                item->value_len = ce->value_len;
                keylist->count++;
            } 

            // traverse through the linked list
            ce = ce->next;
        }
        pthread_mutex_unlock(&c->lock[i]);
    }

    return keylist;
}

uint8_t* serialize_list(const list* l, uint32_t* out_len) {
    
    if(l == NULL || l->count > UINT32_MAX || out_len == NULL) 
        return NULL;

    size_t total = sizeof(uint16_t);

    for (size_t i = 0; i < l->count; i++) {
        const entry *e = &l->items[i];

        if (e->key_len && !e->key)
            return NULL;

        if (e->value_len && !e->value)
            return NULL;

        // Check fixed-size fields (keylen, valuelen) + key length
        if (total > SIZE_MAX - 6 - e->key_len)
            return NULL;

        // keylen is 2 and valuelen is 4 bytes, 2 + 4 = 6
        total += 6 + e->key_len;

        // Check value length
        if (total > SIZE_MAX - e->value_len)
            return NULL;

        total += e->value_len;

        // Response.valuelen is uint32_t
        if (total > UINT32_MAX)
            return NULL;
    }

    uint8_t* buffer = malloc(sizeof(total));

    if(!buffer)
        return NULL;

    // Used uint8_t to make sure pointer arithmetic only increments n bytes
    uint8_t* ptr = buffer;
    uint16_t count = l->count;

    memcpy(ptr, &count, 2);
    ptr += 2;

    for (size_t i = 0; i < l->count; i++) {
        const entry *e = &l->items[i];

        memcpy(ptr, &e->key_len, 2);
        ptr += 2;

        memcpy(ptr, e->key, e->key_len);
        ptr += e->key_len;

        memcpy(ptr, &e->value_len, 4);
        ptr += 4;

        memcpy(ptr, e->value, e->value_len);
        ptr += e->value_len;
    }

    *out_len = (uint32_t)total;
    return buffer;
}