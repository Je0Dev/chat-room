#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "chat.h"
#include "client.h"

// Client state shared between the reader thread and main thread
static int sock;
static int my_id = -1;
static char my_room[MAX_ROOM_NAME] = "lobby";
static volatile sig_atomic_t running = 1;

// Send a message to the server, handling network errors
static void send_all(const char *msg, size_t len) {
    if (send(sock, msg, len, 0) < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            printf("\nLost connection to server\n");
        } else {
            perror("send");
        }
        running = 0;
    }
}

// Reader thread: continuously receives messages from the server
// Runs concurrently with the main thread so messages appear in real-time
void *reader_thread(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];
    ssize_t n;

    while (running && (n = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';

        // First message from server tells us our ID and room
        if (my_id == -1 && strncmp(buffer, "You are Client ", 15) == 0) {
            my_id = atoi(buffer + 15);
            // Parse room name from "You are Client X in #room"
            char *room_start = strstr(buffer, " in #");
            if (room_start) {
                strncpy(my_room, room_start + 5, MAX_ROOM_NAME - 1);
                my_room[MAX_ROOM_NAME - 1] = '\0';
                char *newline = strchr(my_room, '\n');
                if (newline) *newline = '\0';
            }
            printf("\nYou are [Client %d] in [#%s]\n", my_id, my_room);
            printf("[#%s][Client %d]-> ", my_room, my_id);
            fflush(stdout);
            continue;
        }

        // Detect room changes from server confirmations
        if (strncmp(buffer, "Joined #", 8) == 0) {
            strncpy(my_room, buffer + 8, MAX_ROOM_NAME - 1);
            my_room[MAX_ROOM_NAME - 1] = '\0';
            char *newline = strchr(my_room, '\n');
            if (newline) *newline = '\0';
        } else if (strncmp(buffer, "Left room", 9) == 0) {
            // /leave always returns to lobby
            strncpy(my_room, "lobby", MAX_ROOM_NAME - 1);
            my_room[MAX_ROOM_NAME - 1] = '\0';
        }

        // Print incoming message, then reprint prompt so user can keep typing
        printf("\r%s\n", buffer);
        printf("[#%s][Client %d]-> ", my_room, my_id);
        fflush(stdout);
    }

    if (running) {
        printf("\nServer disconnected\n");
        running = 0;
    }
    return NULL;
}

// SIGINT handler: clean exit on Ctrl+C
static void handle_disconnect(int sig) {
    (void)sig;
    running = 0;
    close(sock);
    exit(0);
}

// Entry point: connect to server, spawn reader thread, read input in main thread
void start_client(void) {
    signal(SIGINT, handle_disconnect);
    signal(SIGPIPE, SIG_IGN);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
    };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        exit(1);
    }

    printf("Connected to server. Waiting for ID...\n");

    // Spawn reader thread to receive messages from other clients
    pthread_t reader;
    pthread_create(&reader, NULL, reader_thread, NULL);
    pthread_detach(reader);

    // Main thread: read from stdin and send to server
    char buffer[BUFFER_SIZE];
    while (running && fgets(buffer, sizeof(buffer), stdin) != NULL) {
        send_all(buffer, strlen(buffer));
        printf("[#%s][Client %d]-> ", my_room, my_id);
        fflush(stdout);
    }

    close(sock);
}
