#include "tinycache.h"

ssize_t read_all(int fd, void* buffer, size_t len) {

    size_t total = 0;

    while(total < len) {
        ssize_t n = read(fd, buffer, len);

        if(n == 0)
            return -1;

        if(n < 0) {
            if(errno == EINTR)
                continue;

            return -1;
        }
        total += (size_t)n;
    }

    return 0;
}

ssize_t write_all(int fd, void* buffer, size_t len) {

    size_t total = 0;

    while(total < len) {
        ssize_t n = write(fd, buffer, len);

        if(n == 0)
            return -1;

        if(n < 0) {
            if(errno == EINTR)
                continue;

            return -1;
        }
        total += (size_t)n;
    }

    return 0;
}

