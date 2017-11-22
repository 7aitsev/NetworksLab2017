#include "lib/werror.h"
#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/server.h"
#include "server/terminal/terminal.h"

#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#define SERVER_BACKLOG 5

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

    return rv;
}

void
handle_master_socket(WSAEVENT event)
{
    static int bytes;
    static const int bufsize = 1024;
    static char buf[1024];
    static WSANETWORKEVENTS network_events;
    static struct sockaddr_storage sa_peer;
    static socklen_t sa_peer_len = sizeof(sa_peer);
    
    WSAEnumNetworkEvents(this.master, event, &network_events);
    bytes = recvfrom(this.master, buf, sizeof(buf), 0,
                     (struct sockaddr*) &sa_peer, &sa_peer_len);
    if(0 < bytes)
    {
        buf[bytes] = '\0';

        if(0 < (bytes = handler_new(buf, bufsize, &sa_peer)))
        {
            bytes = sendto(this.master, buf, bytes, 0,
                           (struct sockaddr*) &sa_peer, sa_peer_len);
            logger_log("[server] was sent %d bytes\n", bytes);
            if(-1 == bytes)
            {
                logger_log("[server] sento() failed: %s\n", wstrerror());
                this.is_running = 0;
            }
        }
    }
    else if(0 == bytes)
    {
        //keep alive
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
        DWORD result = WSAWaitForMultipleEvents(2, events, FALSE, 5000, FALSE);
        switch(result)
        {
            case WAIT_TIMEOUT:
                // send keep alive messages to all pears, make cleanup of expired ones
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
}

void
server_destroy()
{
    WSACleanup();
    terminal_stop();

    handler_destroy();
}