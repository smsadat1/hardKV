#include "tinycache.h"

/*
*   Persistence strategy:
*   Hash partitioned append only binary records
*   Each partitions are separate binary files under TINYCACHE_LOCATION/record/ 
*   Reverse scanning is used for fetching values from associated key
*   Because last mutation is all what matters for querying value from key
*/

/*
    Caller does not need to manage disk lock 
    All disk operation functions manages disk lock internally
*/
bool tc_add_record(
    Cache* c, char *key, void *value, size_t value_len
) {

    // Select the record partition to append using hash
    uint64_t partition = sdbm_hash(key) % TC_MAX_RECORD_PARTITION;

    struct stat st;
    char* record_file = NULL;
    if (asprintf(&record_file, "%s/%lu.bin", TINYCACHE_PATH, partition) == -1) {
        tc_log(TC_ERROR, "Failed to read record at partition: %llu", partition);     
        free(record_file);   
        return false;
    }

/*

    On disk record format for SET (optimized for reverse scanning)
    |-----------------------------------------------------------------------------------|
    | value_len bytes | 4 bytes   | key_len bytes | 2 bytes  | 1 byte  | 4 bytes        |
    |-----------------|-----------|---------------|----------|---------|----------------|
    | VALUE           | value_len | key           | key_len  | op      | MAGIC (4bytes) |
    |-----------------|-----------|---------------|----------|---------|----------------|

*/

    // Write (append) operation starts here (lock from here)
    pthread_mutex_lock(&c->disk_lock[partition]);

    int record_fd = open(record_file, O_CREAT | O_WRONLY, 0750);
    if(record_fd == -1) {
        perror("open");
        pthread_mutex_unlock(&c->disk_lock[partition]);
        return false;
    }

    // once pathname is used by open(), it is no longer needed
    free(record_file);

    if(fstat(record_fd, &st) == -1) {
        perror("fstat");
        pthread_mutex_unlock(&c->disk_lock[partition]);
        return false;
    }

    off_t record_offset = st.st_size;    
    uint16_t keylen = (uint16_t)strlen(key);    // exact 2 bytes
    uint32_t valuelen = (uint32_t)value_len;    // exact 4 bytes
    uint8_t op = RECORD_OP_SET;
    uint32_t magic = TC_RECORD_MAGIC;

    pwrite_all(record_fd, value, value_len, record_offset);
    record_offset += (off_t)value_len;

    pwrite_all(record_fd, &valuelen, sizeof(valuelen), record_offset);
    record_offset += (off_t)sizeof(valuelen);

    pwrite_all(record_fd, key, keylen, record_offset);
    record_offset += (off_t)keylen;

    pwrite_all(record_fd, &keylen, sizeof(keylen), record_offset);
    record_offset += (off_t)sizeof(keylen);

    pwrite_all(record_fd, &op, sizeof(op), record_offset);
    record_offset += (off_t)sizeof(op);

    pwrite_all(record_fd, &magic, sizeof(magic), record_offset);

    
    // disk synchronization for persistence
    if(fdatasync(record_fd) == -1) {
        perror("fdatasync");
        pthread_mutex_unlock(&c->disk_lock[partition]);
        return false;
    }

    close(record_fd);
    pthread_mutex_unlock(&c->disk_lock[partition]);
    return true;
}

/*
    Caller does not need to manage disk lock 
    All disk operation functions manages disk lock internally
*/
bool tc_get_record(
    Cache* c, char* key, void** out_value, size_t* valuelen
) {

    // Select the record partition to append using hash
    uint64_t partition = sdbm_hash(key) % TC_MAX_RECORD_PARTITION;
    
    struct stat st;

    char* record_file = NULL;
    if (asprintf(&record_file, "%s/%lu.bin", TINYCACHE_PATH, partition) == -1) {
        tc_log(TC_ERROR, "Failed to read record at partition: %llu", partition);        
        free(record_file);
        return false;
    }

    int record_fd = open(record_file, O_CREAT | O_RDONLY, 0750);
    if(record_fd == -1) {
        perror("open");
        return false;
    }

    // once pathname is used by open(), it is no longer needed
    free(record_file);

    if(fstat(record_fd, &st) == -1) {
        perror("fstat");
        return false;
    }

    off_t record_offset = st.st_size;   

    // Read happens inside revscan()
    pthread_mutex_lock(&c->disk_lock[partition]);
    uint8_t revscan_status = revscan(record_fd, key, strlen(key), record_offset, out_value, valuelen);

    switch (revscan_status)
    {
    case RECORD_READ_ERROR:
        tc_log(TC_ERROR, "Failed to read record at partition: %llu", partition);   
        pthread_mutex_unlock(&c->disk_lock[partition]);     
        return false;
    case RECORD_KEY_NOT_FOUND:
        tc_log(TC_DEBUG, "Key <%s> not found on disk", key);
        pthread_mutex_unlock(&c->disk_lock[partition]);
        return false;
    /* 
    * Not finding value is expected for DELETED 
    * So those are not count as errors
    * break goes to return unlocking disk_lock
    */
    default:
        break;
    }

    pthread_mutex_unlock(&c->disk_lock[partition]);
    return true;
}

/*
    Caller does not need to manage disk lock 
    All disk operation functions manages disk lock internally
*/
bool tc_delete_record(Cache* c, char* key) {

    // Select the record partition to append using hash
    uint64_t partition = sdbm_hash(key) % TC_MAX_RECORD_PARTITION;

    struct stat st;

    char* record_file = NULL;
    if (asprintf(&record_file, "%s/%lu.bin", TINYCACHE_PATH, partition) == -1) {
        tc_log(TC_ERROR, "Failed to read record at partition: %llu", partition); 
        free(record_file);       
        return false;
    }
    
/*

    On disk record format for DELETE (optimized for reverse scanning)
    |-----------------------------------------------------|
    | key_len bytes | 2 bytes  | 1 byte  | 4 bytes        |
    |---------------|----------|---------|----------------|
    | key           | key_len  | op      | MAGIC (4bytes) |
    |---------------|----------|---------|----------------|

*/
    // Write (append) operation starts here (lock from here)
    pthread_mutex_lock(&c->disk_lock[partition]);

    int record_fd = open(record_file, O_CREAT | O_WRONLY, 0750);
    if(record_fd == -1) {
        perror("open");
        pthread_mutex_unlock(&c->disk_lock[partition]);
        return false;
    }

    // once pathname is used by open(), it is no longer needed
    free(record_file);

    if(fstat(record_fd, &st) == -1) {
        perror("fstat");
        pthread_mutex_unlock(&c->disk_lock[partition]);
        return false;
    }

    off_t record_offset = st.st_size;    
    size_t keylen = strlen(key);
    uint8_t op = RECORD_OP_DELETE;
    uint32_t magic = TC_RECORD_MAGIC;

    pwrite_all(record_fd, key, keylen, record_offset);
    record_offset += (off_t)keylen;

    pwrite_all(record_fd, &keylen, sizeof(keylen), record_offset);
    record_offset += (off_t)sizeof(keylen);

    pwrite_all(record_fd, &op, sizeof(op), record_offset);
    record_offset += (off_t)sizeof(op);

    pwrite_all(record_fd, &magic, sizeof(magic), record_offset);

    // disk synchronization for persistence
    if(fdatasync(record_fd) == -1) {
        perror("fdatasync");
        pthread_mutex_unlock(&c->disk_lock[partition]);
        return false;
    }

    close(record_fd);
    pthread_mutex_unlock(&c->disk_lock[partition]);
    return true;
}