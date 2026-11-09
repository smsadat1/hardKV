#ifndef TINYCACHE_H
#define TINYCACHE_H

#define _GNU_SOURCE             // needed for asprintf()
#define _POSIX_C_SOURCE 200809L // needed for strdup() and other POSIX specific functions

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <signal.h>
#include <sys/socket.h> 
#include <sys/stat.h>
#include <sys/types.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <unistd.h>

extern const char *TINYCACHE_PATH;
#define TC_DEFAULT_PATH "/srv/archive/metadata"

#define TC_MAX_READ_BYTES 777 // max_command_len + whitespace + max_key_len + whitespace + max_value_len
#define TC_MAX_COMMAND_LEN 7
#define TC_MAX_KEY_LEN 256
#define TC_MAX_VALUE_LEN 512 
#define TC_DEFAULT_MAX_MEM_MB 124
#define TC_MAX_MEMORY_MB 128
#define TC_MAX_ALLOWED_CONNECTION 80

#define TC_MAX_RECORD_PARTITION 1000
#define TC_RECORD_MAGIC 0xB16B00B5u
#define TC_RECORD_REVSCAN_BLOCK_SIZE 4096

#define TC_SERVER_ERROR -67
#define TC_PORT 9736   // opposite of redis
#define TC_DEFAULT_TTL 300
#define TC_INTERVAL_SEC 5
#define TC_NO_EXPIRY 0
#define TC_BUCKET_COUNT INT16_MAX
#define TC_TTL_EXPIRED -1
#define TC_TTL_NOT_EXIST -2
#define TC_EXPIRY_ILLEGAL -3
#define TC_NEWLINE 0x0A

#define TC_LOG_GREY "\033[38;5;244m"
#define TC_LOG_GREEN "\033[38;5;46m"
#define TC_LOG_YELLOW "\033[38;5;11m"
#define TC_LOG_RED "\033[38;5;203m"
#define TC_LOG_DEEP_RED "\033[1;31m"
#define TC_LOG_COLOR_TERMINATE "\033[0m"


/* =================================================== Logging =================================================== */
typedef enum {
    TC_DEBUG,
    TC_INFO,
    TC_WARNING,
    TC_ERROR,
    TC_FATAL,
} TC_LOG_LEVEL;

void tc_log(TC_LOG_LEVEL level, char* mesg, ...);


/* =================================================== Cache Layer =================================================== */
typedef enum {
    EVICTION_LRU,
    EVICTION_LFU,
} EvictionPolicy;

typedef struct cache_entry_t {
    
    char *key;
    void *value;
    size_t value_len;
        
    // count access frequency for LFU
    uint32_t frequency;    
    // Doubly linked LRU list
    struct cache_entry_t *lru_prev;
    struct cache_entry_t *lru_next;
    
    struct cache_entry_t *next;
} cache_entry;


typedef struct cache_t {
    
    cache_entry** buckets;
    size_t entries;

    // atomic because both eviction worker and main thread needs to access it 
    atomic_size_t mem_used;
    
    size_t mem_max;
    
    EvictionPolicy policy;
    cache_entry* lru_head;
    cache_entry* lru_tail;
    pthread_mutex_t eviction_lock;
    
    // atomic because shutdown state needs to be synced accross threads
    atomic_bool shutdown;
    
    pthread_mutex_t worker_lock;
    pthread_cond_t worker_cond;

    // disk operations lock (per partition)
    pthread_mutex_t* disk_lock;

    // cache lock (per bucket)
    pthread_mutex_t* lock;   
} Cache;


// LFU
cache_entry* find_lfu_victim(Cache* c);

void lru_insert_lock(Cache* c, cache_entry* ce);
void lru_update_lock(Cache* c, cache_entry* ce);
void lru_remove_lock(Cache* c, cache_entry* ce);


void init_cache(Cache* cache);
void destroy_cache(Cache* cache);

// hash function
uint64_t sdbm_hash(const char* key);

// used by SET (thread safe)
bool insert(Cache* c, char* key, void* value, size_t value_len, size_t bucket_idx);
// used by GET, EXPIRE, EXISTS
cache_entry* search(Cache* c, char* key, size_t bucket_idx);
// used by DELETE
bool delete(Cache* c, char* key, size_t bucket_idx);

/* =================================================== Persistent Layer =================================================== */

enum tc_record_op {
    RECORD_OP_SET = 1,
    RECORD_OP_DELETE = 2,
    RECORD_VALUE_FOUND = 3,
    RECORD_VALUE_NOT_FOUND = 4,
    RECORD_KEY_NOT_FOUND = 5,
    RECORD_READ_ERROR = 6,
    RECORD_WRITE_ERROR = 7,
};  

typedef struct record_t {
    void* value;
    uint32_t value_len;

    char* key;
    uint32_t key_len;

    uint8_t op;

    uint32_t crc;
    uint32_t magic;
} record;

ssize_t read_all(int fd, void* buffer, size_t len);
ssize_t write_all(int fd, void* buffer, size_t len);
bool pread_all(int fd, void *buf, size_t count, off_t offset);
bool pwrite_all(int fd, const void *buf, size_t count, off_t offset);

uint8_t revscan(
    int record_fd, 
    const char* target_key, 
    size_t key_len, 
    off_t offset,  
    void **out_value,
    size_t *out_value_len
);

bool tc_add_record(Cache* c,char* key, void* value, size_t value_len);
bool tc_get_record(Cache* c,char* key, void** out_value, size_t* valuelen);
bool tc_delete_record(Cache* c, char* key);


/* =================================================== API Layer =================================================== */

typedef struct command_t {
    char *key;
    char *value;
    size_t value_len;
} command;

enum hkv_command {
    HKV_CMD_SET     = 0,
    HKV_CMD_SETEX   = 1,
    HKV_CMD_GET     = 2,
    HKV_CMD_DELETE  = 3,
    HKV_CMD_EXIST   = 4,
    HKV_CMD_EVICT   = 5,
    HKV_CMD_EXPIRE  = 6,
    HKV_CMD_TTL     = 7,
    HKV_CMD_LIST    = 8,
    HKV_CMD_STAT    = 9,
    HKV_CMD_PING    = 10,     
};

enum hkv_response {
    HKV_STATUS_OK     =  0,
    HKV_STATUS_NFOUND =  1,    // requested key not found
    HKV_STATUS_INCVER =  2,    // incompatible version
    HKV_STATUS_INVCMD =  3,    // invalid command
    HKV_STATUS_INVREQ =  4,    // invalid request
    HKV_STATUS_INTERR =  5,    // internal error  
    HKV_STATUS_DENIED =  6,    // request denied
};

typedef struct {
    uint16_t version;
    uint8_t status;
    uint8_t msg_len;
    char* message;
    uint32_t value_len;
    void* value;
} response;


typedef struct {
    char *key;
    void *value;
    size_t value_len;
} key_result;

typedef struct key_list_t {
    key_result *items;
    size_t count;
} key_list;

bool tc_set(Cache* c, command* str);
void* tc_get(Cache* c, command* str, size_t* value_len);
bool tc_delete(Cache* c, command* str);
bool tc_exists(Cache* c, command* str);
bool tc_evict(Cache* c, command* str);
key_list* tc_get_keylist(Cache* c, const char* pattern);
void tc_free_keylist(key_list* keylist);

typedef struct WorkerConfig_t {
    int max_mem_mb;
    EvictionPolicy policy;
    Cache* cache;
} WorkerConfig;


void* worker(void* arg);
void exit_worker(Cache* cache, pthread_t* worker_thread);

void tc_server(Cache* cache);

// network I/O
void handle_request(int conn_fd, command* cmdbuf);
void send_response(int connd_fd, uint8_t status, char* msg, size_t value_len, void* value);

// command parsing from server
void parse_command(Cache* cache, int conn_fd);


#endif // TINYCACHE_H