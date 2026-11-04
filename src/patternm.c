#include "tinycache.h"


void tc_free_keylist(key_list* keylist) {
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
* But hardKV enforces per bucket locks in cache keeping mutex lock overhead substantially lower
* So KEYS <pattern> style is not a major issue for hardKV
*/
key_list* tc_get_keylist(Cache* c, const char* pattern) {

    key_list* keylist = (key_list*)calloc(1, sizeof(*keylist));

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
                key_result* tmp = (key_result*)realloc(keylist->items, keylist->count+1);
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
                key_result* item = &keylist->items[keylist->count];
                item->key = strdup(ce->key);
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