#include "tinycache.h"

// custom pwrite() wrapper for guaranteeing disk writes

bool pwrite_all(int fd, const void *buf, size_t count, off_t offset) {
    
    const char* ptr_buf = buf;
    size_t written = 0;

    while (written < count)
    {
        ssize_t n = pwrite(fd, ptr_buf + written, count - written, offset + (off_t)written);
        
        if(n < 0) {
            if(errno == EINTR)
                continue;
            
            perror("pwrite");
            return false;
        }
        if(n == 0) 
            return false;
    
        written += (size_t)n;
    }
    
    return true;
}