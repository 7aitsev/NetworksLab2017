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

#define SERVER_HOST NULL
#define SERVER_PORT "5001"
#define SERVER_BACKLOG 5

struct serverdata
{
    char* host;
    char* port;
    int isrunning;
    int listensocket;
    pthread_t accept_tid;
};

static struct serverdata this;

static int
server_bind(struct addrinfo* servinfo)
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
    return sfd;
}

int
server_prepare()
{
    int rv;
    struct addrinfo hints;
    struct addrinfo* servinfo;

    this.host = SERVER_HOST;
    this.port = SERVER_PORT;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    rv = getaddrinfo(this.host, this.port, &hints, &servinfo);
    if(0 != rv)
    {
        logger_log("[server] getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    if(0 <= (rv = server_bind(servinfo)))
    {
        if(-1 == listen(rv, SERVER_BACKLOG))
        {
            logger_log("[server] listen: %s\n", strerror(errno));
            return -1;
        }
        this.listensocket = rv;
    }

    return rv;
}

static void*
server_acceptloop()
{
    int slave;
    int master = this.listensocket;
    struct sockaddr_storage sa_peer;
    socklen_t addrsize = sizeof(sa_peer);

    handler_init();
    this.isrunning = 1;
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
            if(!this.isrunning)
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

    handler_deleteall(lambda(int, (struct peer* p) { return 0 != p->p_tid;}));
    handler_destroy();

    return NULL;
}

void
server_run()
{
    pthread_create(&this.accept_tid, NULL,
            server_acceptloop, &this.listensocket);

    terminal_setstopservercb(&server_stop);
    terminal_run();
}

void
server_stop()
{
    --this.isrunning;
    shutdown(this.listensocket, SHUT_RDWR);
    close(this.listensocket);
}

void
server_join()
{
    pthread_join(this.accept_tid, NULL);
    terminal_join();
}
