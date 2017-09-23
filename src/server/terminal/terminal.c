#include "logger/logger.h"
#include "server/terminal/terminal.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static struct termdata this;

static void*
terminal_loop()
{
    char inpline[10];

    logger_log("[terminal] started\n");
    while(1)
    {
        fgets(inpline, 10, stdin);
        if(0 == strcmp(inpline, "q\n"))
        {
            logger_log("[terminal] shutdown requested\n");
            if(NULL != this.td_stopserver)
            {
                this.td_stopserver();
            }
            else
            {
                logger_log("[terminal] callback == NULL\n");
            }
            break;
        }
    }

    return NULL;
}

void
terminal_setstopservercb(void (*stopserver_cb)(void))
{
    this.td_stopserver = stopserver_cb;
}

void
terminal_run()
{
    logger_log("[terminal] starting...\n");
    pthread_create(&this.td_tid, NULL, terminal_loop, NULL);
}

void
terminal_join()
{
    logger_log("[terminal] joining...\n");
    pthread_join(this.td_tid, NULL);
}
