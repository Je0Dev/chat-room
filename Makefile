CC = gcc
# -Wall -Wextra: enable all warnings so we catch bugs early
# -Werror: treat warnings as errors (forces us to write clean code)
# -pthread: link the POSIX threads library
CFLAGS = -Wall -Wextra -Werror -pthread -Iinclude

all: bin/server bin/client

# Create bin/ directory if it doesn't exist
bin:
	mkdir -p bin

# $@ = target name (bin/server)
# $^ = all prerequisites (source + header files)
SERVER_SRCS = server/main.c server/server.c server/room.c server/commands.c server/client_handler.c server/messaging.c
SERVER_HDRS = server/server.h server/room.h server/commands.h server/client_handler.h server/messaging.h include/chat.h

bin/server: $(SERVER_SRCS) $(SERVER_HDRS) | bin
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRCS)

bin/client: client/main.c client/client.c client/client.h include/chat.h | bin
	$(CC) $(CFLAGS) -o $@ client/main.c client/client.c

clean:
	rm -rf bin

.PHONY: all clean
