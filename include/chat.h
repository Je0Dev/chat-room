#ifndef CHAT_H
#define CHAT_H

// Port number the server listens on (1024-65535 range, below 1024 needs root)
#define SERVER_PORT 8080

// Max bytes we read from a socket in one recv() call
#define BUFFER_SIZE 1024

// Maximum number of clients that can connect simultaneously (used in later stages)
#define MAX_CLIENTS 100
#define MAX_ROOM_NAME 32

#endif
