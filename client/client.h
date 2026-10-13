#ifndef CLIENT_H
#define CLIENT_H

#include "chat.h"

// Starts the client (called from main.c)
// Connects to server, spawns reader thread, runs input loop
void start_client(void);

#endif
