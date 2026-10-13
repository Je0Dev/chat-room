#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "server.h"
#include "client_handler.h"
#include "room.h"
#include "messaging.h"

#define DEFAULT_ROOM "lobby"

// Parse and execute slash commands from clients
// Each command modifies state or sends information back to the requesting client
void handle_command(int sender_id, char *input) {
    struct client_info *sender = find_client_by_id(sender_id);
    if (!sender) return;

    if (strncmp(input, "/help", 5) == 0) {
        // Show all available commands
        char help[] =
            "Available commands:\n"
            "  /list          - Show all online clients\n"
            "  /rooms         - Show all active rooms\n"
            "  /who           - Show who's in your room\n"
            "  /join <room>   - Join a room (creates if new)\n"
            "  /leave         - Leave current room back to #lobby\n"
            "  /msg <id> <text> - Private message (same room only, target must /accept)\n"
            "  /accept        - Accept a pending private message\n"
            "  /decline       - Decline a pending private message\n"
            "  /help          - Show this help\n";
        send_all(sender->fd, help, strlen(help));

    } else if (strncmp(input, "/list", 5) == 0) {
        // List all online clients
        send_user_list(sender->fd);

    } else if (strncmp(input, "/rooms", 6) == 0) {
        // List all active rooms with user counts
        send_room_list(sender->fd);

    } else if (strncmp(input, "/who", 4) == 0) {
        // List clients in the sender's current room
        send_room_members(sender->fd);

    } else if (strncmp(input, "/join ", 6) == 0) {
        // Extract room name and trim trailing whitespace/newline
        char *room_name = input + 6;
        char *end = room_name + strlen(room_name) - 1;
        while (end > room_name && (*end == '\n' || *end == ' ')) *end-- = '\0';

        if (strlen(room_name) == 0 || strlen(room_name) >= MAX_ROOM_NAME) {
            char err[] = "Invalid room name\n";
            send_all(sender->fd, err, strlen(err));
            return;
        }

        // Update the sender's room (protected by mutex)
        pthread_mutex_lock(&clients_mutex);
        strncpy(sender->room, room_name, MAX_ROOM_NAME - 1);
        sender->room[MAX_ROOM_NAME - 1] = '\0';
        pthread_mutex_unlock(&clients_mutex);

        // Confirm to the sender
        char confirm[64];
        int confirm_len = snprintf(confirm, sizeof(confirm), "Joined #%s\n", room_name);
        send_all(sender->fd, confirm, confirm_len);

        // Notify others in the new room that someone joined
        char join_msg[64];
        int join_len = snprintf(join_msg, sizeof(join_msg), "Client %d joined the room\n", sender_id);
        room_broadcast(sender->fd, sender_id, join_msg, join_len);

    } else if (strncmp(input, "/leave", 6) == 0) {
        // Move sender back to the default lobby room
        pthread_mutex_lock(&clients_mutex);
        strncpy(sender->room, DEFAULT_ROOM, MAX_ROOM_NAME - 1);
        sender->room[MAX_ROOM_NAME - 1] = '\0';
        pthread_mutex_unlock(&clients_mutex);

        char confirm[] = "Left room. You are now in #lobby\n";
        send_all(sender->fd, confirm, strlen(confirm));

        // Notify lobby that someone joined
        char leave_msg[64];
        int leave_len = snprintf(leave_msg, sizeof(leave_msg), "Client %d joined the room\n", sender_id);
        room_broadcast(sender->fd, sender_id, leave_msg, leave_len);

    } else if (strncmp(input, "/msg ", 5) == 0) {
        // Parse: /msg <client_id> <message>
        int target_id;
        char *text = strchr(input + 5, ' ');
        if (!text) {
            char err[] = "Usage: /msg <client_id> <message>\n";
            send_all(sender->fd, err, strlen(err));
            return;
        }
        *text = '\0';  // Null-terminate the ID portion
        target_id = atoi(input + 5);
        text++;  // Move past the null to the message text
        // send_private checks the same-room rule and queues the PM as pending
        send_private(sender_id, target_id, text, strlen(text));

    } else if (strncmp(input, "/accept", 7) == 0) {
        // Deliver the oldest pending PM waiting for this client
        accept_pending(sender_id);

    } else if (strncmp(input, "/decline", 8) == 0) {
        // Discard the oldest pending PM waiting for this client
        decline_pending(sender_id);
    }
}
