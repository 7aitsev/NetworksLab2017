#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/terminal/terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <winsock2.h>

enum terminal_action {
    TERMINAL_ACTION_QUIT,
    TERMINAL_ACTION_STATUS,
    TERMINAL_ACTION_KILL
};

struct termdata
{
    HANDLE td_hndl;
    void (*td_stopserver)(void);
    HANDLE td_input_event;
    enum terminal_action td_action;
    peer_t td_peer;
    HANDLE td_read_ena;
    HANDLE td_exec_ena;
};

static struct termdata this;

static void
action(enum terminal_action action)
{
    this.td_action = action;
    if(! SetEvent(this.td_input_event))
    {
        logger_log("[terminal] SetEvent() failed (%ld)\n", GetLastError());
        return;
    }

    if(! ReleaseSemaphore(this.td_exec_ena,  1, NULL))
    {
        logger_log("[termial] release for execution failed: %d\n",
            GetLastError());
        return;
    }
}

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

static void
action_kill()
{
    logger_log("[terminal] kill %hd\n", this.td_peer);
    handler_delete_first_if(lambda(int, (struct peer* p)
    {
        return p->p_id == this.td_peer && p->p_id != 0;
    }));
}

DWORD WINAPI
terminal_loop()
{
    int cmdsize = 10;
    char inpline[cmdsize];

    logger_log("[terminal] started\n");
    do
    {
        if(WAIT_FAILED == WaitForSingleObject(this.td_read_ena, INFINITE))
        {
            logger_log("[terminal] wait for reading failed: %ld",
                GetLastError());
            return -1;
        }

        printf("> ");
        fgets(inpline, cmdsize, stdin);
        if(0 == strcmp(inpline, "q\n"))
        {
            action(TERMINAL_ACTION_QUIT);
            break;
        }
        else if(0 == strcmp(inpline, "status\n"))
        {
            action(TERMINAL_ACTION_STATUS);
        }
        else if(1 == sscanf(inpline, "k %hd\n", &this.td_peer))
        {
            action(TERMINAL_ACTION_KILL);
        }
        else
        {
            if(! ReleaseSemaphore(this.td_read_ena,  1, NULL))
            {
                logger_log("[termial] release for reading failed: %d\n",
                    GetLastError());
                return -1;
            }
        }
    }
    while(1);

    return 0;
}

int
terminal_handle_action()
{
    if(WAIT_FAILED == WaitForSingleObject(this.td_exec_ena, INFINITE))
    {
        logger_log("[terminal] wait for execution failed: %ld",
            GetLastError());
        return -1;
    }

    switch(this.td_action)
    {
        case TERMINAL_ACTION_QUIT:
            action_quit();
            break;
        case TERMINAL_ACTION_STATUS:
            action_show_status();
            break;
        case TERMINAL_ACTION_KILL:
            action_kill();
            break;
        default:
            logger_log("[terminal] Unexpected action\n");
    }

    if(! ResetEvent(this.td_input_event))
    {
        logger_log("[terminal] Cannot reset event\n");
    }
    this.td_action = -1;
    if(! ReleaseSemaphore(this.td_read_ena,  1, NULL))
    {
        logger_log("[termial] release for reading failed: %d\n",
            GetLastError());
        return -1;
    }
    return 0;
}

HANDLE
terminal_get_input_event()
{
    return this.td_input_event;
}

int
terminal_run(void (*stopserver_cb)(void))
{
    this.td_stopserver = stopserver_cb;

    this.td_input_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(NULL == this.td_input_event) 
    { 
        logger_log("[terminal] CreateEvent failed (%ld)\n", GetLastError());
        return -1;
    }

    this.td_read_ena = CreateSemaphore(NULL, 1, 1, NULL);
    this.td_exec_ena = CreateSemaphore(NULL, 0, 1, NULL);
    if(NULL == this.td_read_ena || NULL == this.td_exec_ena)
    {
        logger_log("[terminal] failed to create semaphores\n");
        return -1;
    }

    logger_log("[terminal] starting...\n");
    this.td_hndl = CreateThread(NULL, 0, terminal_loop, NULL, 0, NULL);
    return 0;
}

void
terminal_stop()
{
    CloseHandle(this.td_input_event);
    CloseHandle(this.td_read_ena);
    CloseHandle(this.td_exec_ena);
    logger_log("[terminal] joining...\n");
    WaitForSingleObject(this.td_hndl, INFINITE);
}