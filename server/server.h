#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <signal.h>
#include "chat.h"

// Shared struct definition so both server.c and room.c can use it
struct client_info {
    int fd;
    int id;
    pthread_t tid;
    char room[MAX_ROOM_NAME];
};

// Shared state defined in server.c, accessed by room.c and client_handler.c
extern struct client_info *clients[MAX_CLIENTS];
extern int client_count;
extern pthread_mutex_t clients_mutex;
extern volatile sig_atomic_t running;
extern void send_all(int fd, const char *msg, size_t len);

// Starts the server (called from main.c)
// Blocks forever accepting connections until SIGINT
void start_server(void);

#endif
