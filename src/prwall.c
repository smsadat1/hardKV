#include "tinycache.h"

// custom pread() wrapper for guaranteeing disk writes
bool pread_all(int fd, void *buf, size_t count, off_t offset) {
    
    char* ptr_buf = buf;
    size_t received = 0;

    while (received < count)
    {
        ssize_t n = pread(fd, ptr_buf + received, count - received, offset + (off_t)received);

        if(n < 0) {
            if(errno == EINTR) 
                continue;

            tc_log(TC_ERROR, "%s | fd=%d offset=%jd size=%zd", strerror(errno), fd, (intmax_t)offset, n);
            perror("pread");
            return false;
        }
        if(n == 0)   // EOF came before receiving all requested bytes.
            return false;

        received += (size_t)n;
    }
    return true;
}


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