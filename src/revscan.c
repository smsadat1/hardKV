#include "tinycache.h"

/*
    Revers scanning technique:
    Since last mutation is all what matters
    So scan happens in reverse to find the latest mutation earliest 
    Record format is optimized for this reverse scanning
*/

uint8_t revscan(
    int record_fd, 
    const char* target_key, 
    size_t key_len, 
    off_t offset,  
    void **out_value,
    size_t *out_value_len
) {

    // start from EOF
    off_t count = offset;
    *out_value = NULL;
    *out_value_len = 0;

    // tc_log(TC_DEBUG, "START count=%jd", (intmax_t)count);


    while(count > 0) {
        
        uint32_t magic;
        if(!pread_all(record_fd, &magic, sizeof(magic), count - (off_t)sizeof(magic)))
            return RECORD_READ_ERROR;

        // tc_log(TC_DEBUG, "MAGIC=%08" PRIx32 " count=%jd", magic, (intmax_t)count);

        count -= (off_t)sizeof(magic);
        // continue to find records afterwards 
        if(magic != TC_RECORD_MAGIC)
            continue;   

        uint8_t op;
        if(!pread_all(record_fd, &op, sizeof(op), count - (off_t)sizeof(op)))
            return RECORD_READ_ERROR;

        // tc_log(TC_DEBUG, "OP=%u count=%jd", op, (intmax_t)count);

        count -= (off_t)sizeof(op);
        // when OP is unknown (may induce by disk hardware corruption)
        // continue to find records afterwards 
        if(op != RECORD_OP_DELETE && op != RECORD_OP_SET)
            continue;   

        /*
            Read key:
            First read the keylen on disk
            Then read the key itself
         */
        uint16_t keylen;
        if(!pread_all(record_fd, &keylen, sizeof(keylen), count - (off_t)sizeof(keylen)))
            return RECORD_READ_ERROR;
        
        // tc_log(TC_DEBUG, "KEYLEN=%u count=%jd", keylen, (intmax_t)count);

        // keylen stored as 2 bytes
        count -= (off_t)sizeof(keylen); 
        char key[keylen+1];
        
        if(!pread_all(record_fd, key, keylen, count - (off_t)keylen))
            return RECORD_READ_ERROR;
        
        key[keylen] = '\0';
        count -= (off_t)keylen;

        if (keylen != key_len || memcmp(key, target_key, keylen) != 0) {
            
            /*
                When key does not match
                For SET: move (value + valuelen + key + keylen) amount backwards
                For DELETE: move (key + keylen) amount backwards
            */
            if(op == RECORD_OP_SET) {
                uint32_t valuelen;
                // valuelen is stored as 4 bytes
                if(!pread_all(record_fd, &valuelen, sizeof(valuelen), count - (off_t)sizeof(valuelen)))
                    return RECORD_READ_ERROR;
                
                count -= (off_t)sizeof(valuelen) + (off_t)valuelen;
            }   

            // Then continue to find record afterwards when key doesn't match
            continue;
        }
        // tc_log(TC_DEBUG, "KEY=%s count=%jd", key, (intmax_t)count);

        // when last mutation is DELETE then value doesn't matter
        if(op ==  RECORD_OP_DELETE) {
            *out_value = NULL;
            *out_value_len = 0;
            return RECORD_VALUE_NOT_FOUND;
        }

        /*
            Read value (in case of key matched and OP == SET):
            First read the valuelen on disk
            Then read the value itself
            Then return the value
         */
        
        uint32_t valuelen;
        // valuelen is stored as 4 bytes
        if(!pread_all(record_fd, &valuelen, sizeof(valuelen), count-(off_t)sizeof(valuelen))) 
            return RECORD_READ_ERROR;

        // tc_log(TC_DEBUG, "VALUELEN=%" PRIu32 " count=%jd", valuelen, (intmax_t)count);

        count -= (off_t)sizeof(valuelen);

        void *value = malloc(valuelen ? valuelen : 1);
        if (value == NULL)
            return RECORD_READ_ERROR;
        
        if(!pread_all(record_fd, value, valuelen, count-(off_t)valuelen)) {
            free(value);
            return RECORD_READ_ERROR;
        }

        *out_value = value;
        *out_value_len = valuelen;
        return RECORD_VALUE_FOUND;
    }

    // when key is not found
    return RECORD_KEY_NOT_FOUND;
}