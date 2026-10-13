#include <stdlib.h>
#include "server.h"

// main.c is intentionally minimal - all server logic lives in server.c
// This keeps the entry point clean and makes the codebase easier to navigate

int main(void) {
    start_server();
    return 0;
}
