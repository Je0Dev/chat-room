#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "chat.h"
#include "server.h"
#include "client_handler.h"

// Server state shared across all client threads
struct client_info *clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static int next_client_id = 0;
volatile sig_atomic_t running = 1;
static int server_fd = -1;

// Send a raw message to a client socket, handling network errors gracefully
// EPIPE/ECONNRESET means the peer disconnected - not an error we need to report
void send_all(int fd, const char *msg, size_t len) {
    if (send(fd, msg, len, 0) < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            printf("[Client disconnected unexpectedly]\n");
        } else {
            perror("send");
        }
    }
}

// SIGINT handler: sets running=0 so all threads exit their loops
// Then closes all client sockets and joins threads for a clean exit
static void shutdown_server(int sig) {
    (void)sig;
    printf("\nShutting down server...\n");
    running = 0;
    close(server_fd);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        close(clients[i]->fd);
        pthread_join(clients[i]->tid, NULL);
    }
    pthread_mutex_unlock(&clients_mutex);

    printf("Server stopped.\n");
    exit(0);
}

// Entry point: sets up signal handlers, creates the listening socket,
// and runs the accept loop forever (until SIGINT)
void start_server(void) {
    signal(SIGINT, shutdown_server);
    signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe (client disconnected mid-send)

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    // SO_REUSEADDR lets us restart immediately without waiting for the OS to release the port
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(SERVER_PORT),
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        exit(1);
    }

    printf("Server listening on port %d\n", SERVER_PORT);

    // Accept loop: wait for connections, spawn a thread for each
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) break;  // accept() interrupted by signal
            perror("accept");
            continue;
        }

        // Allocate client info on the heap so each thread gets its own copy
        struct client_info *info = malloc(sizeof(*info));
        info->fd = client_fd;
        info->id = next_client_id++;

        if (pthread_create(&info->tid, NULL, handle_client, info) != 0) {
            perror("pthread_create");
            free(info);
            close(client_fd);
            continue;
        }
    }
}
