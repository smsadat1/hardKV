// TCP server for receiving commands

#include "tinycache.h"

void send_result(int conn_fd, void* result, size_t resultlen) {
    
    if (write(conn_fd, result, resultlen) == -1) {
        tc_log(TC_ERROR, "Write failed");
        return;
    }
}