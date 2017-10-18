#include "logger/logger.h"
#include "server/handler/peer/peer.h"

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
peer_destroy(struct peer* p)
{
    peer_closesocket(p->p_sfd);
    memset(p, 0, sizeof(struct peer));
}

int
peer_isexist(struct peer* p)
{
    return 0 != p->p_tid;
}

int
peer_isnotexist(struct peer* p)
{
    return 0 == p->p_tid;
}

void
peer_closesocket(int sfd)
{
    shutdown(sfd, SHUT_RDWR);
    close(sfd);
}
