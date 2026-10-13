#ifndef ROOM_H
#define ROOM_H

#include "chat.h"
#include <stddef.h>

// Forward declaration - the full struct is defined in main.c
struct client_info;

// Send a message to all clients in the same room as the sender
void room_broadcast(int sender_fd, int sender_id, const char *msg, size_t len);

// Send list of all active rooms with user counts to a client
void send_room_list(int fd);

// Send list of clients in the requester's current room
void send_room_members(int fd);

// Find a client by its socket file descriptor
struct client_info *find_client_by_fd(int fd);

#endif
