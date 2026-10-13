#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "server.h"

// Thread function: handles all communication with a single connected client
void *handle_client(void *arg);

// Register a new client in the shared clients array (thread-safe)
void add_client(struct client_info *info);

// Remove a client from the shared clients array (thread-safe)
void remove_client(int fd);

// Look up a client by their numeric ID (thread-safe)
struct client_info *find_client_by_id(int id);

#endif
