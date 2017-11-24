#include "lib/werror.h"
#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/server.h"
#include "server/terminal/terminal.h"

#include <stdio.h>
#include <string.h>
#include <winsock2.h>

/* from "service" module */
extern const int g_period; // determines by protocol
extern char* g_buf; // is going to be allocated in server_init()
extern const int g_bufsize; // determines by protocol

struct serverdata
{
    const char* host;
    const char* port;
    int is_running;
    SOCKET master;
};

static struct serverdata this;

static int
trybind(struct addrinfo* servinfo)
{
    struct addrinfo* p;
    BOOL yes = TRUE;

    for(p = servinfo; NULL != p; p = p->ai_next)
    {
        this.master = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(INVALID_SOCKET == this.master)
        {
            continue;
        }

        if(0 != setsockopt(this.master, SOL_SOCKET, SO_REUSEADDR,
            (char*) &yes, sizeof(yes)))
        {
            logger_log("[server] setsockopt: %s\n", wstrerror());
            return -1;
        }

        if(0 == bind(this.master, p->ai_addr, p->ai_addrlen))
        {
            break;
        }

        closesocket(this.master);
    }

    if(NULL == p)
    {
        logger_log("[server] Could not bind: %s\n", wstrerror());
        return -1;
    }

    freeaddrinfo(servinfo);
    return 0;
}

int
server_init(const char* host, const char* port)
{
    int rv;
    struct addrinfo hints;
    struct addrinfo* servinfo;

    WSADATA wsaData;
    if(0 != WSAStartup(MAKEWORD(2,2), &wsaData)) {
        logger_log("WSAStartup() failed: %s\n", wstrerror());
        return -1;
    }

    this.host = host;
    this.port = port;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_UDP;

    rv = getaddrinfo(this.host, this.port, &hints, &servinfo);
    if(0 != rv)
    {
        logger_log("[server] getaddrinfo(): %s\n", gai_strerror(rv));
        return -1;
    }

    rv = trybind(servinfo);
    if(0 == rv && -1 == terminal_run(server_stop))
        return -1;

    g_buf = malloc(g_bufsize);
    return rv;
}

void
handle_master_socket(WSAEVENT event)
{
    static int bytes;
    static WSANETWORKEVENTS network_events;
    static struct sockaddr_storage sa_peer;
    static socklen_t sa_peer_len = sizeof(sa_peer);
    
    WSAEnumNetworkEvents(this.master, event, &network_events);
    bytes = recvfrom(this.master, g_buf, g_bufsize, 0,
                     (struct sockaddr*) &sa_peer, &sa_peer_len);
    if(0 < bytes)
    {
        g_buf[bytes] = '\0';

        logger_log("[server] received \"%s\"\n", g_buf);
        if(0 < (bytes = handler_new_request(&sa_peer)))
        {
            bytes = sendto(this.master, g_buf, bytes, 0,
                           (struct sockaddr*) &sa_peer, sa_peer_len);
            if(-1 == bytes)
            {
                logger_log("[server] sento() failed: %s\n", wstrerror());
                this.is_running = 0;
            }
        }
    }
    else if(0 == bytes)
    {
        handler_touch_peer(&sa_peer);
        if(-1 == sendto(this.master, NULL, 0, 0,
                (struct sockaddr*) &sa_peer, sa_peer_len))
        {
            logger_log("sendto failed while sending heart beat\n");
            this.is_running = 0;
        }
    }
    else
    {
        logger_log("[server] recvfrom failed: %s\n", wstrerror());
        this.is_running = 0;
    }
}

void
server_run()
{
    handler_init();

    WSAEVENT events[2] = {WSACreateEvent(), terminal_get_input_event()};
    WSAEventSelect(this.master, events[0], FD_READ);

    this.is_running = 1;
    while(this.is_running)
    {
        DWORD result =
            WSAWaitForMultipleEvents(2, events, FALSE, g_period, FALSE);
        switch(result)
        {
            case WAIT_TIMEOUT:
                handler_remove_expired();
                break;
            case WAIT_OBJECT_0:
                handle_master_socket(events[0]);
                break;
            case WAIT_OBJECT_0 + 1:
                if(-1 == terminal_handle_action())
                    this.is_running = 0;
                break;
            case WAIT_FAILED:
                logger_log("the function has failed: %ld\n", GetLastError());
                this.is_running = 0;
                break;
            default:
                logger_log("unexpected case %ld\n", result);
                this.is_running = 0;
        }
    }
    CloseHandle(events[0]);
}

void
server_stop()
{
    this.is_running = 0;
    closesocket(this.master);
    free(g_buf);
}

void
server_destroy()
{
    WSACleanup();
    terminal_stop();

    handler_destroy();
}