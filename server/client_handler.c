#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include "client_handler.h"
#include "server.h"
#include "room.h"
#include "commands.h"
#include "messaging.h"

#define DEFAULT_ROOM "lobby"

// Register a new client in the shared clients array
// Mutex ensures only one thread modifies the list at a time
void add_client(struct client_info *info) {
    pthread_mutex_lock(&clients_mutex);
    if (client_count < MAX_CLIENTS) {
        clients[client_count++] = info;
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Remove a client from the shared clients array
// Shifts remaining elements to fill the gap
void remove_client(int fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->fd == fd) {
            free(clients[i]);
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Find a client by their numeric ID (linear search through the clients array)
struct client_info *find_client_by_id(int id) {
    struct client_info *result = NULL;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->id == id) {
            result = clients[i];
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return result;
}

// Thread function: runs for each connected client
// Receives messages, logs them, broadcasts to room or executes commands
void *handle_client(void *arg) {
    struct client_info *info = arg;
    int fd = info->fd;
    int id = info->id;

    // Every new client starts in the default lobby room
    strncpy(info->room, DEFAULT_ROOM, MAX_ROOM_NAME - 1);
    info->room[MAX_ROOM_NAME - 1] = '\0';

    printf("[Client %d] connected (joined #%s)\n", id, DEFAULT_ROOM);
    add_client(info);

    // Tell the client their assigned ID and current room
    char id_msg[64];
    int id_len = snprintf(id_msg, sizeof(id_msg), "You are Client %d in #%s\n", id, DEFAULT_ROOM);
    send_all(fd, id_msg, id_len);

    char buffer[BUFFER_SIZE];
    ssize_t n;

    // Main receive loop: block on recv(), process each incoming message
    while (running && (n = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';

        // Server-side logging: [time] [Client X] [#room] message
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[9];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        struct client_info *me = find_client_by_fd(fd);
        printf("[%s] [Client %d] [#%s] %s", time_str, id, me ? me->room : "?", buffer);

        // Commands start with '/', everything else is a chat message
        if (buffer[0] == '/') {
            handle_command(id, buffer);
        } else {
            room_broadcast(fd, id, buffer, n);
        }
    }

    if (n < 0 && errno != ECONNRESET) {
        perror("recv");
    }

    // Notify others in the room that this client has left
    struct client_info *me = find_client_by_fd(fd);
    if (me) {
        char leave_msg[64];
        int leave_len = snprintf(leave_msg, sizeof(leave_msg), "Client %d left the room\n", id);
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < client_count; i++) {
            if (clients[i]->fd != fd && strcmp(clients[i]->room, me->room) == 0) {
                send_all(clients[i]->fd, leave_msg, leave_len);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    printf("[Client %d] disconnected\n", id);
    remove_pending_for(id);  // Drop any pending PMs involving this client
    remove_client(fd);
    close(fd);
    return NULL;
}
