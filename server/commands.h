#ifndef COMMANDS_H
#define COMMANDS_H

// Parse and execute slash commands (/help, /list, /join, /leave, /msg, etc.)
void handle_command(int sender_id, char *input);

#endif
