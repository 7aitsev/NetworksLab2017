#include "config/config.h"
#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/server.h"
#include "server/terminal/terminal.h"

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

struct serverdata
{
    char* host;
    char* port;
    int backlog;
    int proto;
    int socktype;

    int isrunning;
    int sockethndl;
    pthread_t accept_tid;

    void (*runserver)(void);
    void (*stopserver)(void);
    void (*joinserver)(void);
};

static struct serverdata this;

#define is_udp() (this.proto == IPPROTO_UDP)
#define is_tcp() (this.proto == IPPROTO_TCP)

void
server_sethost(char* hostname)
{
    this.host = hostname;
}

void
server_setport(char* port)
{
    this.port = port;
}

void
server_setbacklog(int backlog)
{
    this.backlog = backlog;
}

void
server_setprotocol(int proto)
{
    this.proto = (proto == SERVER_UDP) ? IPPROTO_UDP : IPPROTO_TCP;
    this.socktype = (proto == SERVER_UDP) ? SOCK_DGRAM : SOCK_STREAM;
}

static int
trybind(struct addrinfo* servinfo, int* binded_socket)
{
    struct addrinfo* p;
    int yes = 1;
    int sfd;

    for(p = servinfo; NULL != p; p = p->ai_next)
    {
        if(-1 == (sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)))
        {
            continue;
        }

        if(-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
        {
            logger_log("[server] setsockopt: %s\n", strerror(errno));
            return -1;
        }

        if(0 == bind(sfd, p->ai_addr, p->ai_addrlen))
        {
            break;
        }

        close(sfd);
    }

    if(p == NULL)
    {
        logger_log("[server] Could not bind: %s\n", strerror(errno));
        return -1;
    }

    freeaddrinfo(servinfo);
    *binded_socket = sfd;
    return 0;
}

int
prepare_server()
{
    int rv;
    int sfd = -1;
    struct addrinfo hints;
    struct addrinfo* servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = this.socktype;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = this.proto;

    rv = getaddrinfo(this.host, this.port, &hints, &servinfo);
    if(0 != rv)
    {
        logger_log("[server] getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    rv = trybind(servinfo, &sfd);
    if(0 == rv && is_tcp())
    {
        rv = listen(sfd, this.backlog); 
        if(-1 == rv)
        {
            logger_log("[server] listen: %s\n", strerror(errno));
            return -1;
        }
    }

    this.sockethndl = sfd;
    return rv;
}

static void*
server_acceptloop()
{
    int slave;
    int master = this.sockethndl;
    struct sockaddr_storage sa_peer;
    socklen_t addrsize = sizeof(sa_peer);

    handler_init();
    __sync_fetch_and_or(&this.isrunning, 1);
    while(1)
    {
        slave = accept(master, (struct sockaddr*) &sa_peer, &addrsize);
        if(-1 != slave)
        {
            logger_log("[server] new peer: %d\n", slave);
            handler_new(slave);
        }
        else
        {
            if(!__sync_and_and_fetch(&this.isrunning, 1))
            {
                break;
            }
            else if(EINTR != errno)
            {
                logger_log("[server] accept(): %s\n", strerror(errno));
                return NULL;
            }
        }
    }

    handler_destroy();

    return NULL;
}

void
run_tcp_server()
{
    pthread_create(&this.accept_tid, NULL, server_acceptloop, NULL);

    terminal_setstopservercb(&server_stop);
    terminal_run();
}

void
stop_tcp_server()
{
    __sync_fetch_and_and(&this.isrunning, 0);
    shutdown(this.sockethndl, SHUT_RDWR);
    close(this.sockethndl);
}

void
join_tcp_server()
{
    pthread_join(this.accept_tid, NULL);
    terminal_join();
}

void
run_udp_server()
{
    int bytes;
    char buf[100];
    struct sockaddr_storage thathost;
    socklen_t thathost_len = sizeof(thathost);
    while(1)
    {
        bytes = recvfrom(this.sockethndl, buf, sizeof(buf), 0,
                         (struct sockaddr*) &thathost, &thathost_len);
        if(0 < bytes)
        {
            buf[bytes] = '\0';
            if(0 != strcmp(buf, "OFF\n"))
            {
                logger_log("[udp] %d, recv:%s\n", this.sockethndl, buf);
            }
            else
            {
                break;
            }

            sendto(this.sockethndl, "I got your message", 18, MSG_NOSIGNAL,
                   (struct sockaddr*) &thathost, thathost_len);
        }
        else if(0 == bytes)
        {
            break;
        }
        else
        {
            logger_log("[udp] recvfrom failed: %s\n", strerror(errno));
            break;
        }
    }
}

void
stop_udp_server()
{
    logger_log("[server] stop udp server\n");
}

void
join_udp_server()
{
    logger_log("[server] join udp server\n");
}

void
server_run()
{
    this.runserver();
}

void
server_stop()
{
    this.stopserver();
}

void
server_join()
{
    this.joinserver();
}

int
server_prepare()
{
    logger_log("[config] arguments\n"
            "\ttype=%d, host=%s, port=%s, backlog=%d\n",
            this.proto, this.host, this.port, this.backlog);

    switch(this.proto)
    {
        case IPPROTO_TCP:
            this.runserver = &run_tcp_server;
            this.stopserver = &stop_tcp_server;
            this.joinserver = &join_tcp_server; 
            break;
        default:
            this.runserver = &run_udp_server;
            this.stopserver = &stop_udp_server;
            this.joinserver = &join_udp_server;
    }
    return prepare_server();
}
