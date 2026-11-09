#include "tinycache.h"

/*
    Response format:
    
    -------------------------------------------------------------------------------    
    |  2 bytes  | 1 byte |  1 byte  | msg_len bytes |  4 bytes  | value_len bytes |
    |-----------|--------|----------|---------------|-----------|-----------------|
    |  version  | status |  msg_len |    message    | value_len |     value       |     
    -------------------------------------------------------------------------------
*/

void send_response(
    int connd_fd, uint8_t status, char* msg, size_t value_len, void* value
) {

    write_all(connd_fd, "01", 2);
    write_all(connd_fd, &status, 1);
    
    size_t msglen = strlen(msg);
    write_all(connd_fd, &msglen, 1);
    write_all(connd_fd, msg, msglen);

    write_all(connd_fd, &value_len, 4);
    if(value_len <= 0) {
        write_all(connd_fd, NULL, 0);
        return;
    } 

    write_all(connd_fd, value, value_len);
}


/*
    Request format 
    -----------------------------------------------------------------------------------------------------------------
    |  2 bytes  |   1 byte   | tenant_len bytes |  1 byte   | 2 bytes | key_len bytes | 4 bytes   | value_len bytes |
    |-----------|------------|------------------|-----------|---------|---------------|-----------|-----------------|
    |  version  | tenant_len |    tenant_id     |  command  | key_len |      key      | value_len |     value       |
    -----------------------------------------------------------------------------------------------------------------

    For commands with no required key or values, it will be zero lenght field
*/

void handle_request(int conn_fd, command* cmdbuf) {

    char* version;
    read_all(conn_fd, version, 2);

    if(memcmp(version, "01", 2) != 0)
        send_response(conn_fd, HKV_STATUS_INCVER, "Incompatible version", 0, NULL);
    
    uint8_t tenant_id_len;
    read_all(conn_fd, &tenant_id_len, 1);
    
    char* tenant_id;
    read_all(conn_fd, tenant_id, tenant_id_len);
    if(!verify_tenant_id(tenant_id)) 
        send_response(conn_fd, HKV_STATUS_DENIED, "Only alphanumeric is allowed for tenant_id", 0, NULL);

    uint8_t command;
    read_all(conn_fd, &command, 1);
    size_t keylen;

    cmdbuf->key = NULL;
    cmdbuf->value_len = 0;
    cmdbuf->value = NULL;
    
    switch (command)
    {
    case HKV_CMD_SET:

        read_all(conn_fd, &keylen, 2);
        read_all(conn_fd, cmdbuf->key, keylen);

        read_all(conn_fd, &cmdbuf->value_len, 4);
        read_all(conn_fd, &cmdbuf->value, cmdbuf->value_len);
        break;
    
    case HKV_CMD_GET:
        read_all(conn_fd, &keylen, 2);
        read_all(conn_fd, cmdbuf->key, keylen);
        break;
    
    case HKV_CMD_DELETE:
        read_all(conn_fd, &keylen, 2);
        read_all(conn_fd, cmdbuf->key, keylen);
        break;
    
    case HKV_CMD_EVICT:
        read_all(conn_fd, &keylen, 2);
        read_all(conn_fd, cmdbuf->key, keylen);
        break;

    case HKV_CMD_EXIST:
        read_all(conn_fd, &keylen, 2);
        read_all(conn_fd, cmdbuf->key, keylen);
        break;
    
    case HKV_CMD_LIST:
        // here key is prefix for match
        read_all(conn_fd, &keylen, 2);
        read_all(conn_fd, cmdbuf->key, keylen);
        break;

    case HKV_CMD_STAT:
        // here key is for STAT state (MEM or KEY)
        read_all(conn_fd, &keylen, 2);
        read_all(conn_fd, cmdbuf->key, keylen);
        break;
    
    case HKV_CMD_PING:
        // key not needed for PING so 0 length
        read_all(conn_fd, 0, 2);
        read_all(conn_fd, cmdbuf->key, 0);
        break;

    default:
        send_response(conn_fd, HKV_STATUS_INVCMD, "Unknown or invalid command", 0, NULL);
        break;
    }
}
