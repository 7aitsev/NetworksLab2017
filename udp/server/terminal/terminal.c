#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/terminal/terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <winsock2.h>

struct termdata
{
    HANDLE td_hndl;
    void (*td_stopserver)(void);
};

static struct termdata this;

static void
action_quit()
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
action_show_status()
{
    logger_log("[terminal] showing statistics\n");
    printf("Online peers: %d\nServed peers for all time: %d\n",
            handler_getcurrent(), handler_gettotal());
    handler_foreach(&peer_printinfo);
}
/*
static void
action_kill(peer_t peer)
{
    logger_log("[terminal] kill %hd\n", peer);
    handler_delete_first_if(
            lambda(int, (struct peer* p)
                {return p->p_id == peer && p->p_id != 0;}
            ));
}
*/
DWORD WINAPI
terminal_loop()
{
    // peer_t peer;
    int cmdsize = 10;
    char inpline[cmdsize];

    logger_log("[terminal] started\n");
    do
    {
        printf("> ");
        fgets(inpline, cmdsize, stdin);
        if(0 == strcmp(inpline, "q\n"))
        {
            action_quit();
            break;
        }
        else if(0 == strcmp(inpline, "status\n"))
        {
            action_show_status();
        }/*
        else if(1 == sscanf(inpline, "k %hd\n", &peer))
        {
            terminal_action_kill(peer);
        }*/
    }
    while(1);

    return 0;
}

void
terminal_run(void (*stopserver_cb)(void))
{
    this.td_stopserver = stopserver_cb;
    logger_log("[terminal] starting...\n");
    this.td_hndl = CreateThread(NULL, 0, terminal_loop, NULL, 0, NULL);
}

void
terminal_stop()
{
    logger_log("[terminal] joining...\n");
    // pthread_cancel(this.td_tid); // to be shure
    WaitForSingleObject(this.td_hndl, INFINITE);
}