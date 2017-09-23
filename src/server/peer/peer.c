#include "logger/logger.h"
#include "server/peer/peer.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void
peer_printinfo(struct peer* p)
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof addr;
    int port;
    char ipstr[INET_ADDRSTRLEN];

    getpeername(p->p_sfd, (struct sockaddr*) &addr, &len);

    if(AF_INET == addr.ss_family)
    {
        struct sockaddr_in *s = (struct sockaddr_in*) &addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

        printf("Peer #%d\n\tIP address: %s\n\tPort: %d\n\t"
                "Socket: %d\n",
                p->p_id, ipstr, port, p->p_sfd);
    }
    else
    {
        printf("Peer#%d has unsupported adress family\n", p->p_id);
    }
}

void
peer_set(struct peer* to, const struct peer* from)
{
    to->p_id = from->p_id;
    to->p_tid = from->p_tid;
    to->p_sfd = from->p_sfd;
}

void
peer_setargs(struct peer* p,
        const peer_t id, const pthread_t tid, const int sfd)
{
    p->p_id = id;
    p->p_tid = tid;
    p->p_sfd = sfd;
}

void
peer_shutdown(struct peer* p)
{
    shutdown(p->p_sfd, SHUT_RDWR);
    close(p->p_sfd);
}
