#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/server.h"
#include "server/terminal/terminal.h"

#include <stdio.h>
#include <string.h>
#include <ws2tcpip.h>
#include <winsock2.h>

#define SERVER_BACKLOG 5

struct serverdata
{
    const char* host;
    const char* port;
    int isrunning;
    SOCKET master;
};

static struct serverdata this;

char*
error()
{
    static char buf[1024];
    if(0 != FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(), 0, buf, 1024, NULL))
    {
        sprintf(buf, "FormatMessage() failed: err=0x%lx\n", GetLastError());
    }
    return buf;
}

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
            logger_log("[server] setsockopt: %s\n", error());
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
        logger_log("[server] Could not bind: %s\n", error());
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
        logger_log("WSAStartup() failed: %s\n", error());
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
    if(rv == 0)
    {
        logger_log("[server] starting terminal\n");
        terminal_run(server_stop);
    }

    return rv;
}

void
server_run()
{
    handler_init();

    int bytes;
    int bufsize = 1024;
    char buf[bufsize];
    struct sockaddr_storage sa_peer;
    socklen_t sa_peer_len = sizeof(sa_peer);

    __sync_fetch_and_or(&this.isrunning, 1);
    while(1)
    {
        bytes = recvfrom(this.master, buf, sizeof(buf), 0,
                         (struct sockaddr*) &sa_peer, &sa_peer_len);
        if(0 < bytes)
        {
            buf[bytes] = '\0';
            
            handler_new(buf, bufsize, &sa_peer);

            bytes = sendto(this.master, "I got your message", 18, 0,
                           (struct sockaddr*) &sa_peer, sa_peer_len);
            if(-1 == bytes)
            {
                logger_log("[server] sento() failed: %s\n", error());
            }
        }
        else if(0 == bytes)
        {
            break;
        }
        else
        {
            if(!__sync_and_and_fetch(&this.isrunning, 1))
            {
                break;
            }
            logger_log("[server] recvfrom failed: %s\n", error());
            break;
        }
    }
}

void
server_stop()
{
    __sync_fetch_and_and(&this.isrunning, 0);
    closesocket(this.master);
}

void
server_destroy()
{
    WSACleanup();
    terminal_stop();

    handler_destroy();
}