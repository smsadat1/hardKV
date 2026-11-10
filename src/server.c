// TCP server for receiving commands

#include "tinycache.h"

volatile sig_atomic_t keep_running = 1;


void tc_handle_sigint() {
    keep_running = 0;
}

void* hkv_handle_client(void* args) {

    ClientConfig* config = (ClientConfig*)args;
    Cache* cache = config->cache;
    int conn_fd = config->conn_fd;
    free(config);

    handle_request(cache, conn_fd);

    close(conn_fd);
    return NULL;
}

void tc_server(Cache* cache)
{
    int sock_fd, conn_fd;
    struct sockaddr_in server_addr = {0}, client = {0}; 
    struct sigaction sa;

    tc_log(TC_INFO, "Initializing tinycache server...");

    tc_log(TC_DEBUG, "Setting up signal handler");
    sa.sa_handler = &tc_handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);


    tc_log(TC_DEBUG, "Creating socket");
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        tc_log(TC_FATAL, "Socket creation failed");
        close(sock_fd);
        return;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(TC_PORT);

    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    tc_log(TC_DEBUG, "Binding socket");
    if(bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        tc_log(TC_FATAL, "Socket bind failed");
        close(sock_fd);
        return;
    } 

    tc_log(TC_DEBUG, "Start listening socket");
    if( listen(sock_fd, TC_MAX_ALLOWED_CONNECTION) == -1) {
        tc_log(TC_FATAL, "Socket listen failed");
        close(sock_fd);
        return;
    }
    tc_log(TC_INFO, "Listening at port: %d...", TC_PORT);

    socklen_t len = sizeof(client);
    
    while (keep_running)
    {
        conn_fd = accept(sock_fd, (struct sockaddr*)&client, &len);
        if(conn_fd == -1) {
            if(errno == EINTR && !keep_running) 
                break;  // SIGINT interrupted accept()
            
            if (errno == EINTR)
                continue;  // Retry if another signal interrupted accept()
            
            tc_log(TC_ERROR, "Accept failed");
            continue;
        }

        ClientConfig* client_cfg = (ClientConfig*)malloc(sizeof(ClientConfig));
        if(client_cfg == NULL) {
            close(conn_fd);
            continue;
        }
        client_cfg->cache = cache;
        client_cfg->conn_fd = conn_fd;

        // 1 thread per client
        pthread_t client_thread;
        int ret = pthread_create(&client_thread, NULL, hkv_handle_client, client_cfg);
        if (ret != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(ret));
            free(client_cfg);
            close(conn_fd);
            continue;
        }
        pthread_detach(client_thread);
    }

    tc_log(TC_INFO, "Closing connection bye bye...");
    close(sock_fd);
    
    return;
}
