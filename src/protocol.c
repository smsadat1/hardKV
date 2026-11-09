#include "tinycache.h"

/*
    Response format:
    
    -------------------------------------------------------------------------------    
    |  2 bytes  | 1 byte |  1 byte  | msg_len bytes |  4 bytes  | value_len bytes |
    |-----------|--------|----------|---------------|-----------|-----------------|
    |  version  | status |  msg_len |    message    | value_len |     value       |     
    -------------------------------------------------------------------------------
*/

void send_response(int connd_fd, Response* res) {

    write_all(connd_fd, "01", 2);
    write_all(connd_fd, &res->status, 1);
    
    write_all(connd_fd, &res->msglen, 1);
    write_all(connd_fd, &res->msg, &res->msglen);

    write_all(connd_fd, &res->valuelen, 4);
    if(res->valuelen <= 0) {
        write_all(connd_fd, NULL, 0);
        return;
    } 

    write_all(connd_fd, res->value, res->valuelen);
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

void handle_request(Cache* c, int conn_fd) {

    Request* req;
    req->keylen = 0;
    req->key = "";
    req->valuelen = 0;
    req->value = NULL;

    Response* res;
    res->status = HKV_STATUS_OK;
    res->msglen = 0;
    res->msg = "";
    res->valuelen = 0;
    res->valuelen = NULL;


    char* version;
    read_all(conn_fd, version, 2);

    if(memcmp(version, "01", 2) != 0) {
        
        res->status = HKV_STATUS_INCVER;
        res->msglen = 21;
        res->msg = "Incompatible version";
        
        send_response(conn_fd, res);
        return;
    }
    
    uint8_t tenant_id_len;
    read_all(conn_fd, &tenant_id_len, 1);
    
    char* tenant_id;
    read_all(conn_fd, tenant_id, tenant_id_len);
    if(!verify_tenant_id(tenant_id)) {

        res->status = HKV_STATUS_DENIED;
        res->msglen = 43;
        res->msg = "Only alphanumeric is allowed for tenant_id";

        send_response(conn_fd, res);
        return;
    }

    uint8_t command;
    read_all(conn_fd, &command, 1);
    
    switch (command)
    {
    case HKV_CMD_SET:

        read_all(conn_fd, &req->keylen, 2);
        read_all(conn_fd, req->key, req->keylen);

        read_all(conn_fd, &req->valuelen, 4);
        read_all(conn_fd, &req->value, req->valuelen);

        hkv_set(c, req, res);
        send_response(conn_fd, res);
        break;
    
    case HKV_CMD_GET:
        read_all(conn_fd, &req->keylen, 2);
        read_all(conn_fd, req->key, req->keylen);

        hkv_get(c, req, res);
        send_response(conn_fd, res);
        break;
    
    case HKV_CMD_DELETE:
        read_all(conn_fd, &req->keylen, 2);
        read_all(conn_fd, req->key, req->keylen);

        hkv_delete(c, req, res);
        send_response(conn_fd, res);
        break;
    
    case HKV_CMD_EVICT:
        read_all(conn_fd, &req->keylen, 2);
        read_all(conn_fd, req->key, req->keylen);

        hkv_evict(c, req, res);
        send_response(conn_fd, res);
        break;

    case HKV_CMD_EXIST:
        read_all(conn_fd, &req->keylen, 2);
        read_all(conn_fd, req->key, req->keylen);
        
        hkv_exists(c, req, res);
        send_response(conn_fd, res);
        break;
    
    case HKV_CMD_LIST:
        // here key is prefix for match
        read_all(conn_fd, &req->keylen, 2);
        read_all(conn_fd, req->key, req->keylen);

        // hkv_list(c, req, res);
        send_response(conn_fd, res);
        break;

    case HKV_CMD_STAT:
        // here key is for STAT state (MEM or KEY)
        read_all(conn_fd, &req->keylen, 2);
        read_all(conn_fd, req->key, req->keylen);

        hkv_stat(c, req, res);
        send_response(conn_fd, res);
        break;
    
    case HKV_CMD_PING:
        // key not needed for PING so 0 length
        read_all(conn_fd, 0, 2);
        read_all(conn_fd, req->key, 0);

        res->status = HKV_STATUS_OK;
        res->msglen = 5;
        res->msg = "PING";
        send_response(conn_fd, res);
        break;

    default:
        res->status = HKV_STATUS_INVCMD;
        res->msglen = 27;
        res->msg = "Unknown or invalid command";
        send_response(conn_fd, res);
        break;
    }
}
