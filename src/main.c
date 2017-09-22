#include "logger/logger.h"
#include "server/server.h"

int
main(void)
{
    logger_init();

    int master = server_prepare();
    if(-1 != master)
    {
        server_run(master);
    }
    else
    {
        logger_log("[main] server has not start\n");
    }

    logger_destroy();

    return 0;
}
