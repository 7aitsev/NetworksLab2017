#include "logger/logger.h"

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

int
server_prepare()
{
    int status;
    char* port = "5001";
    char* host = NULL;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    int sfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    if(0 != (status = getaddrinfo(host, port, &hints, &servinfo)))
    {
        logger_log("[server] getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    int yes = 1;
    for(p = servinfo; NULL != p; p = p->ai_next)
    {
        if(-1 == (sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)))
        {
            continue;
        }

        if(-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
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

    if(-1 == listen(sfd, 3))
    {
        logger_log("[server] listen: %s\n", strerror(errno));
        return -1;
    }

    return sfd;
}

int
terminal_isclosed()
{
    return 1;
}

static void*
server_acceptloop(void* pmaster)
{
    int master = *((int*) pmaster);
    int slave;
    struct sockaddr_storage client;
    socklen_t addr_size = sizeof(client);

    while(1)
    {
        slave = accept(master, (struct sockaddr*) &client, &addr_size);
        if(-1 != slave)
        {
            logger_log("[server] new client: %d\n", slave);
            close(slave);
        }
        else
        {
            if(terminal_isclosed())
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

    return NULL;
}

static void*
terminal_loop(void* pmaster)
{
    int master = *((int*) pmaster);
    char inpline[10];

    while(1)
    {
        fgets(inpline, 10, stdin);
        if(0 == strcmp(inpline, "q\n"))
        {
            logger_log("[terminal] shutdown requested\n");
            shutdown(master, SHUT_RDWR);
            close(master);
            break;
        }
    }

    return NULL;
}

void
server_run(int master)
{
    pthread_t accept_tid;
    pthread_t terminal_tid;

    pthread_create(&accept_tid, NULL, server_acceptloop, &master);
    pthread_create(&terminal_tid, NULL, terminal_loop, &master);
    pthread_join(accept_tid, NULL);
    pthread_join(terminal_tid, NULL);
}
