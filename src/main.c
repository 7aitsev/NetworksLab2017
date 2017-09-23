#include "logger/logger.h"
#include "server/server.h"

int
main(void)
{
    logger_init();

    if(-1 != server_prepare())
    {
        logger_log("[main] starting the server...\n");
        server_run();
    }
    else
    {
        logger_log("[main] server has not start\n");
    }

    server_join();
    logger_log("[main] server has shut down\n");
    logger_destroy();

    return 0;
}
