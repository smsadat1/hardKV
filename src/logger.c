// Logger for TinyCache

#include "tinycache.h"

void tc_log(TC_LOG_LEVEL level, char* mesg, ...) {
    
    char timestr[200];
    time_t t;
    struct tm tm_info;

    t = time(NULL);
    if(localtime_r(&t, &tm_info) == NULL) {
        perror("localtime");
        exit(EXIT_FAILURE);
    }

    if (strftime(timestr, sizeof(timestr), "%d/%m/%Y %H:%M:%S", &tm_info) == 0) {
        fprintf(stderr, "strftime returned 0");
        exit(EXIT_FAILURE);
    }

    va_list args;
    va_start(args, mesg);

    flockfile(stdout);

    switch (level)
    {
#ifdef TS_DB_DBG
    case TC_DEBUG:
        printf("[%sDBG%s][%s%s%s] ", 
            TC_LOG_GREEN, TC_LOG_COLOR_TERMINATE,
            TC_LOG_GREY, timestr, TC_LOG_COLOR_TERMINATE
        );    
        break;
        
#endif
    case TC_INFO:
        printf("[INF][%s%s%s] ", 
            TC_LOG_GREY, timestr, TC_LOG_COLOR_TERMINATE
        );    
        break;

    case TC_WARNING:
        printf("[%sWRN%s][%s%s%s] %s", 
            TC_LOG_YELLOW, TC_LOG_COLOR_TERMINATE, 
            TC_LOG_GREY, timestr, TC_LOG_COLOR_TERMINATE, TC_LOG_YELLOW
        );
        break;

    case TC_ERROR:
        printf("[%sERR%s][%s%s%s] %s", 
            TC_LOG_RED, TC_LOG_COLOR_TERMINATE, 
            TC_LOG_GREY, timestr, TC_LOG_COLOR_TERMINATE, TC_LOG_RED
        );
        break;

    case TC_FATAL:
        printf("[%sFTL%s][%s%s%s] %s", 
            TC_LOG_DEEP_RED, TC_LOG_COLOR_TERMINATE,
            TC_LOG_GREY, timestr, TC_LOG_COLOR_TERMINATE, TC_LOG_DEEP_RED
        );
        exit(-1);    

    default:
        break;
    }

    vprintf(mesg, args);
    printf("%s\n", TC_LOG_COLOR_TERMINATE); // terminate set color
    funlockfile(stdout);

    va_end(args);
}
