#include "logger/logger.h"
#include "server/server.h"
#include <stdio.h>

int
main(int argc, char** argv)
{
    if(3 != argc)
    {
        printf("Usage: %s host port\n", argv[0]);
        return 1;
    }

    logger_init();

    if(-1 != server_init(argv[1], argv[2]))
    {
        logger_log("[main] starting the server...\n");
        server_run();
    }
    else
    {
        logger_log("[main] server has not started\n");
    }

    server_destroy();
    logger_log("[main] server has shut down\n");
    logger_destroy();

    return 0;
}