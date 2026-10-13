#include <stdio.h>
#include <string.h>
#include <time.h>
#include "messaging.h"
#include "server.h"
#include "client_handler.h"

// Format a chat message with timestamp and sender ID
// Output format: "[HH:MM:SS] Client X: message"
size_t format_message(char *out, size_t out_size, int sender_id, const char *msg, size_t msg_len) {
    // Get current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[9];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    // Write "[HH:MM:SS] Client X: " prefix, then append the original message
    int prefix_len = snprintf(out, out_size, "[%s] Client %d: ", time_str, sender_id);
    if ((size_t)prefix_len >= out_size) prefix_len = out_size - 1;
    size_t copy_len = msg_len;
    if (prefix_len + copy_len >= out_size) copy_len = out_size - prefix_len - 1;
    memcpy(out + prefix_len, msg, copy_len);
    out[prefix_len + copy_len] = '\0';
    return prefix_len + copy_len;
}

// One private message waiting for its recipient to /accept or /decline
struct pending_msg {
    int from_id;
    int to_id;
    int active;
    char text[BUFFER_SIZE];
};

// Shared queue of pending PMs, protected by its own mutex
static struct pending_msg pending[MAX_CLIENTS];
static pthread_mutex_t pending_mutex = PTHREAD_MUTEX_INITIALIZER;

// Queue a PM for the target and notify both sides
// Only called after the caller verified sender and target share a room
static void queue_pending(int sender_id, int target_id, const char *msg, size_t len) {
    struct client_info *sender = find_client_by_id(sender_id);
    struct client_info *target = find_client_by_id(target_id);
    if (!sender || !target) return;

    // Store the message in the first free slot
    int stored = 0;
    pthread_mutex_lock(&pending_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!pending[i].active) {
            pending[i].from_id = sender_id;
            pending[i].to_id = target_id;
            pending[i].active = 1;
            size_t copy = len;
            if (copy >= BUFFER_SIZE) copy = BUFFER_SIZE - 1;
            memcpy(pending[i].text, msg, copy);
            pending[i].text[copy] = '\0';
            stored = 1;
            break;
        }
    }
    pthread_mutex_unlock(&pending_mutex);

    if (!stored) {
        char err[] = "Too many pending messages, try again later\n";
        send_all(sender->fd, err, strlen(err));
        return;
    }

    // Ask the target whether they want to see the message
    char notify[96];
    int nlen = snprintf(notify, sizeof(notify),
        "Client %d wants to give you a message. Type /accept to view or /decline to ignore\n",
        sender_id);
    send_all(target->fd, notify, nlen);

    // Tell the sender we are waiting for a response
    char waiting[64];
    int wlen = snprintf(waiting, sizeof(waiting),
        "Message pending. Waiting for Client %d to respond...\n", target_id);
    send_all(sender->fd, waiting, wlen);
}

// Send a private message from one specific client to another
// PMs only work inside a room: target must be in the same room as sender
void send_private(int sender_id, int target_id, const char *msg, size_t len) {
    struct client_info *sender = find_client_by_id(sender_id);
    struct client_info *target = find_client_by_id(target_id);

    if (!target) {
        // Target not found, tell the sender
        if (sender) {
            char err[64];
            int err_len = snprintf(err, sizeof(err), "Client %d not found\n", target_id);
            send_all(sender->fd, err, err_len);
        }
        return;
    }
    if (!sender) return;

    // Room check: both must be in the same room (read under mutex)
    // The error does NOT reveal which room the target is in
    int same_room;
    pthread_mutex_lock(&clients_mutex);
    same_room = strcmp(sender->room, target->room) == 0;
    pthread_mutex_unlock(&clients_mutex);

    if (!same_room) {
        char err[] = "You cannot send to this client. You have to join this room first.\n";
        send_all(sender->fd, err, strlen(err));
        return;
    }

    queue_pending(sender_id, target_id, msg, len);
}

// Deliver the oldest pending PM addressed to this client
void accept_pending(int client_id) {
    struct pending_msg pm;
    int found = 0;

    // Take the first active pending message for this client
    pthread_mutex_lock(&pending_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (pending[i].active && pending[i].to_id == client_id) {
            pm = pending[i];
            pending[i].active = 0;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&pending_mutex);

    if (!found) {
        struct client_info *me = find_client_by_id(client_id);
        if (me) {
            char err[] = "No pending messages\n";
            send_all(me->fd, err, strlen(err));
        }
        return;
    }

    // Format and deliver the message
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[9];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    char formatted[BUFFER_SIZE + 64];
    int prefix_len = snprintf(formatted, sizeof(formatted), "[%s] [PM from Client %d]: ",
        time_str, pm.from_id);
    if ((size_t)prefix_len >= sizeof(formatted)) prefix_len = sizeof(formatted) - 1;
    size_t copy_len = strlen(pm.text);
    if (prefix_len + copy_len >= sizeof(formatted)) copy_len = sizeof(formatted) - prefix_len - 1;
    memcpy(formatted + prefix_len, pm.text, copy_len);
    formatted[prefix_len + copy_len] = '\0';

    struct client_info *me = find_client_by_id(client_id);
    if (me) send_all(me->fd, formatted, prefix_len + copy_len);

    // Tell the sender the message was accepted
    struct client_info *from = find_client_by_id(pm.from_id);
    if (from) {
        char note[64];
        int nlen = snprintf(note, sizeof(note), "Client %d accepted your message\n", client_id);
        send_all(from->fd, note, nlen);
    }
}

// Discard the oldest pending PM addressed to this client
void decline_pending(int client_id) {
    struct pending_msg pm;
    int found = 0;

    pthread_mutex_lock(&pending_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (pending[i].active && pending[i].to_id == client_id) {
            pm = pending[i];
            pending[i].active = 0;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&pending_mutex);

    struct client_info *me = find_client_by_id(client_id);
    if (!found) {
        if (me) {
            char err[] = "No pending messages\n";
            send_all(me->fd, err, strlen(err));
        }
        return;
    }

    if (me) {
        char note[64];
        int nlen = snprintf(note, sizeof(note), "Message from Client %d declined\n", pm.from_id);
        send_all(me->fd, note, nlen);
    }

    // Tell the sender the message was declined
    struct client_info *from = find_client_by_id(pm.from_id);
    if (from) {
        char note[64];
        int nlen = snprintf(note, sizeof(note), "Client %d declined your message\n", client_id);
        send_all(from->fd, note, nlen);
    }
}

// Remove all pending messages sent by or to a disconnected client
void remove_pending_for(int client_id) {
    pthread_mutex_lock(&pending_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (pending[i].active &&
            (pending[i].from_id == client_id || pending[i].to_id == client_id)) {
            pending[i].active = 0;
        }
    }
    pthread_mutex_unlock(&pending_mutex);
}

// Build and send the list of all online clients showing their current rooms
void send_user_list(int fd) {
    char list[BUFFER_SIZE * 2];
    int offset = 0;
    offset += snprintf(list + offset, sizeof(list) - offset, "Online clients:\n");

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        offset += snprintf(list + offset, sizeof(list) - offset,
            "  Client %d (in #%s)\n", clients[i]->id, clients[i]->room);
    }
    pthread_mutex_unlock(&clients_mutex);

    send_all(fd, list, offset);
}
