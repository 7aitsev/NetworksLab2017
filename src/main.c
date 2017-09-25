#include "config/config.h"
#include "logger/logger.h"
#include "server/server.h"

int
main(int argc, char** argv)
{
    logger_init();

    config_server(argc, argv);

    if(-1 != server_prepare())
    {
        logger_log("[main] starting the server...\n");
        server_run();
    }
    else
    {
        logger_log("[main] server has not started\n");
    }

    server_join();
    logger_log("[main] server has shut down\n");
    logger_destroy();

    return 0;
}
