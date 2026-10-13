#ifndef MESSAGING_H
#define MESSAGING_H

#include <stddef.h>

// Format: "[HH:MM:SS] Client X: message"
size_t format_message(char *out, size_t out_size, int sender_id, const char *msg, size_t msg_len);

// Send a private message from one client to another
// Only allowed if both clients are in the same room
// The message is queued as pending until the target /accepts or /declines
void send_private(int sender_id, int target_id, const char *msg, size_t len);

// Deliver the oldest pending private message to the client
void accept_pending(int client_id);

// Discard the oldest pending private message for the client
void decline_pending(int client_id);

// Drop all pending messages involving a client that disconnected
void remove_pending_for(int client_id);

// Send list of all online clients (with their rooms) to a requesting client
void send_user_list(int fd);

#endif
