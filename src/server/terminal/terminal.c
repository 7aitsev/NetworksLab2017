#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/terminal/terminal.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static struct termdata this;

static void
terminal_action_quit()
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
}

static void
terminal_action_show_status()
{
    logger_log("[terminal] showing statistics\n");
    printf("Online peers: %d\nServed peers for all time: %d\n",
            handler_getcurrent(), handler_gettotal());
    handler_foreach(&peer_printinfo);
}

static void
terminal_action_kill(peer_t peer)
{
    logger_log("[terminal] kill %hd\n", peer);
    handler_delete_first_if(
            lambda(int, (struct peer* p)
                {return p->p_id != 0 && p->p_id == peer;}
            ));
}

static void*
terminal_loop()
{
    peer_t peer;
    int cmdsize = 10;
    char inpline[cmdsize];

    logger_log("[terminal] started\n");
    printf("> ");
    while(1)
    {
        fgets(inpline, cmdsize, stdin);
        if(0 == strcmp(inpline, "q\n"))
        {
            terminal_action_quit();
            break;
        }
        else if(0 == strcmp(inpline, "status\n"))
        {
            terminal_action_show_status();
        }
        else if(1 == sscanf(inpline, "k %hd\n", &peer))
        {
            terminal_action_kill(peer);
        }
        printf("> ");
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
    pthread_cancel(this.td_tid); // to be shure
    pthread_join(this.td_tid, NULL);
}
