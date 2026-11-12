#include "tinycache.h"

/*
    Authentication request format:
    -------------------------------------------------------------------------------------------
    |  2 bytes  | 1 byte  |    1 byte     | usrname_len bytes |  1 byte    | passwd_len bytes |
    |-----------|---------|---------------|-------------------|------------|------------------|
    |  version  | command |  usrname_len  |      username     | passwd_len |     password     |     
    -------------------------------------------------------------------------------------------

*/

// call once a connection at beginning
bool authenticate_client(ClientConfig* cfg) {
    return false;
}

// call before every command execution except AUTH and PING
bool access_verify(AuthContext* auth) {
    return false;
}
