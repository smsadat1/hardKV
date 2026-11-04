// command parser

#include "tinycache.h"

/*
TODO: Add prefix support in GET, & DELETE
*/
void parse(int conn_fd, Cache* cache, char commandbuf[]) {

    // replace newline with null terminator
    char* newline = strchr(commandbuf, '\n');
    if(newline) {
        *newline = '\0';
    }

    char* command_type = strtok(commandbuf, " ");

    if(strcmp(command_type, "SET") == 0) {
        
        char* key = strtok(NULL, " ");
        char* value = strtok(NULL, " ");

        command c = {
            .key = key,
            .value = value,
            .value_len = value ? strlen(value) : 0,
            .ttl = TC_NO_EXPIRY,
        };

        if (c.key == NULL || c.value == NULL) {
            tc_log(TC_DEBUG, "Send: %s", "FAILED");
            send_result(conn_fd, "FAILED", sizeof("FAILED")-1);
            return;
        }

        if(tc_set(cache, &c)) 
        {
            tc_log(TC_DEBUG, "Send: %s", "OK");
            send_result(conn_fd, "OK", sizeof("OK")-1);
        }
        else {
            tc_log(TC_DEBUG, "Send: %s", "FAILED");
            send_result(conn_fd, "FAILED", sizeof("FAILED")-1);
        }
    } 
    else if(strcmp(command_type, "GET") == 0) {

        command c = {
           .key = strtok(NULL, " "),
           .value = "", .value_len = 0, .ttl = 0,
        };

        size_t value_len = 0;
        void* value = tc_get(cache, &c, &value_len);
        if (value == NULL) 
        {
            tc_log(TC_DEBUG, "Send: %s", "(nil)");
            send_result(conn_fd, "(nil)", sizeof("(nil)")-1);
        }
        else {
            tc_log(TC_DEBUG, "Send: %s", (char*)value);
            send_result(conn_fd, value, value_len);
            free(value);
        }
    } 
    else if(strcmp(command_type, "LIST") == 0) {

        command c = {
            // key is actually the pattern here
           .key = strtok(NULL, " "),
           .value = "", .value_len = 0, .ttl = 0,
        };

        key_list* keylist = tc_get_keylist(cache, c.key);
        if (keylist == NULL) 
        {
            tc_log(TC_DEBUG, "Send: %s", "(nil)");
            send_result(conn_fd, "(nil)", sizeof("(nil)")-1);
        }
        else {
            tc_log(TC_DEBUG, "Send key list with prefix: %s", c.key);
            send_result_list(conn_fd, keylist);
            tc_free_keylist(keylist);
        }
    } 
    else if(strcmp(command_type, "DELETE") == 0) {
        
        command c = {
            .key = strtok(NULL, " "),
            .value = "", .value_len = 0, 
            .ttl = 0,  // no worries, delete() ignores ttl
        };

        if(tc_delete(cache, &c)) 
        {
            tc_log(TC_DEBUG, "Send: %s", "OK");
            send_result(conn_fd, "OK", sizeof("OK")-1);
        }
        else {
            tc_log(TC_DEBUG, "Send: %s", "FAILED");
            send_result(conn_fd, "FAILED", sizeof("FAILED")-1);
        }
    } 
    else if(strcmp(command_type, "EXIST") == 0) {
        
        command c = {
            .key = strtok(NULL, " "),
            .value = "", .value_len = 0, 
            .ttl = 0,
        };

        char yes = 'Y';
        char no = 'N';

        if(tc_exists(cache, &c)) {
            tc_log(TC_DEBUG, "Send: key<%s> exist", c.key);
            send_result(conn_fd, &yes, 1);
        } 
        else {
            tc_log(TC_DEBUG, "Send: key<%s> does not exist", c.key);
            send_result(conn_fd, &no, 1);
        } 
    }
    else if(strcmp(command_type, "EVICT") == 0) {
        
        char* key = strtok(NULL, " ");

        command c = {
            .key = key,
            .value = "", .value_len = 0, 
        };

        if(tc_evict(cache, &c)) 
        {
            tc_log(TC_DEBUG, "Send: %s", "OK");
            send_result(conn_fd, "OK", sizeof("OK") - 1);
        }
        else {
            tc_log(TC_DEBUG, "Send: %s", "FAILED");
            send_result(conn_fd, "FAILED", sizeof("FAILED")-1);       
        }
    }
    else if(strcmp(command_type, "PING") == 0 || 
        strcmp(command_type, "PING\n") == 0 ) {
        send_result(conn_fd, "PONG", sizeof("PONG")-1);
    }
    else if(strcmp(command_type, "STAT") == 0 || 
        strcmp(command_type, "STAT\n") == 0 ) {
        
        char response[16];
        char* stat_type = strtok(NULL, "");

        if(strcmp(stat_type, "KEY") == 0) {
            size_t cache_key_count = cache->entries;
            int len = snprintf(response, sizeof(response), "%zu", cache_key_count);
            send_result(conn_fd, &response, (size_t)len);
        } 
        else if (strcmp(stat_type, "MEM") == 0)
        {
            size_t cache_mem_bytes = atomic_load_explicit(
                &cache->mem_used, memory_order_relaxed
            );
            
            int len = snprintf(response, sizeof(response), "%zu", cache_mem_bytes);
            send_result(conn_fd, &response, (size_t)len);
        }
        else
            send_result(conn_fd, "UNKNOWN", sizeof("UNKNOWN")-1);
    } 
    else {
        // unknown command
        send_result(conn_fd, "UNKNOWN", sizeof("UNKNOWN")-1);
    }
}

void parse_command(Cache* cache, int conn_fd) {

    // extra 1 bytes for newline delimiter
    char command_buff[TC_MAX_READ_BYTES+1] = {0};
    ssize_t n;

    while (true)
    {
        n = read(conn_fd, command_buff, TC_MAX_READ_BYTES);
        
        if(n > 0) {
            command_buff[n-1] = '\0'; // replace newline andnull terminate each command
            tc_log(TC_DEBUG, "Receive: %s", command_buff);
            parse(conn_fd, cache, command_buff);
        }
        else if (n == 0)
            return;
        else 
            perror("read");
    }
}