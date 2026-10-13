#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "room.h"
#include "server.h"
#include "chat.h"

// Find a client by its socket file descriptor
struct client_info *find_client_by_fd(int fd) {
    struct client_info *result = NULL;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->fd == fd) {
            result = clients[i];
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return result;
}

// Send a message to all clients in the same room as the sender
void room_broadcast(int sender_fd, int sender_id, const char *msg, size_t len) {
    // Get current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[9];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    // Format: "[HH:MM:SS] Client X: message"
    char formatted[BUFFER_SIZE + 32];
    int prefix_len = snprintf(formatted, sizeof(formatted), "[%s] Client %d: ", time_str, sender_id);
    if ((size_t)prefix_len >= sizeof(formatted)) prefix_len = sizeof(formatted) - 1;
    size_t copy_len = len;
    if (prefix_len + copy_len >= sizeof(formatted)) copy_len = sizeof(formatted) - prefix_len - 1;
    memcpy(formatted + prefix_len, msg, copy_len);
    size_t formatted_len = prefix_len + copy_len;
    formatted[formatted_len] = '\0';

    // Find the sender's room
    struct client_info *sender = find_client_by_fd(sender_fd);
    if (!sender) return;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        // Only send to clients in the same room
        if (strcmp(clients[i]->room, sender->room) == 0) {
            send_all(clients[i]->fd, formatted, formatted_len);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send list of all active rooms with user counts
void send_room_list(int fd) {
    char list[BUFFER_SIZE * 2];
    int offset = 0;
    offset += snprintf(list + offset, sizeof(list) - offset, "Active rooms:\n");

    pthread_mutex_lock(&clients_mutex);
    // Track which rooms we've already printed
    for (int i = 0; i < client_count; i++) {
        int already_listed = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(clients[i]->room, clients[j]->room) == 0) {
                already_listed = 1;
                break;
            }
        }
        if (!already_listed) {
            // Count users in this room
            int count = 0;
            for (int j = 0; j < client_count; j++) {
                if (strcmp(clients[i]->room, clients[j]->room) == 0) count++;
            }
            offset += snprintf(list + offset, sizeof(list) - offset,
                "  #%s (%d users)\n", clients[i]->room, count);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    send_all(fd, list, offset);
}

// Send list of clients in the requester's current room
void send_room_members(int fd) {
    struct client_info *me = find_client_by_fd(fd);
    if (!me) return;

    char list[BUFFER_SIZE * 2];
    int offset = 0;
    offset += snprintf(list + offset, sizeof(list) - offset, "Clients in #%s:\n", me->room);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i]->room, me->room) == 0) {
            offset += snprintf(list + offset, sizeof(list) - offset, "  Client %d\n", clients[i]->id);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    send_all(fd, list, offset);
}
