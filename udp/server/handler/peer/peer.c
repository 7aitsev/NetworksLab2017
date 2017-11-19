#include "logger/logger.h"
#include "server/handler/peer/peer.h"

#include <stdio.h>

int
peer_touch_cache(struct peer* p)
{
    if(0 == p->p_port)
    {
        logger_log("[peer] caching an address and a port\n");
        p->p_port = ntohs(PEER_PORT(p));

        if(0 != getnameinfo((struct sockaddr*) &p->p_addr, sizeof(struct sockaddr_in),
                    p->p_ipstr, INET_ADDRSTRLEN,
                    NULL, 0, NI_NUMERICHOST))
        {
            logger_log("[peer] getnameinfo() failed\n");
            return -1;
        }
    }
    return 0;
}

void
peer_printinfo(struct peer* p)
{
    if(-1 == peer_touch_cache(p))
        return;

    printf("Peer #%d\n\tIP address: %s\n\tPort: %d\n",
            p->p_id, p->p_ipstr, p->p_port);

    if(PEER_NO_PERMS != p->p_mode)
    {
        printf("\tUsername: %s\n\tCWD: %s\n\tMode: %d\n",
                p->p_username, p->p_cwdpath, p->p_mode);
    }
    else
    {
        printf("\tNot authorised\n");
    }
}

int
peer_check_family(struct sockaddr_storage* addr)
{
    return (AF_INET == addr->ss_family) ? 0 : -1;
}

int
peer_are_addrs_equal(struct sockaddr_in* a, struct sockaddr_in* b)
{
    if(SOCK_ADDR_IN_PORT(a) == SOCK_ADDR_IN_PORT(b)
        && SOCK_ADDR_IN_ADDR(a) == SOCK_ADDR_IN_ADDR(b))
    {
        return 1;
    }
    return 0;
}

int
peer_is_exist(struct peer* p)
{
    return 0 != PEER_PORT(p);
}

int
peer_is_not_exist(struct peer* p)
{
    return 0 == PEER_PORT(p);
}

void
peer_cpy_addr(struct peer* p, struct sockaddr_in* addr)
{
    memcpy(&p->p_addr, addr, sizeof(struct sockaddr_in));
}

void
peer_destroy(struct peer* p)
{
    // close file p_cwd
    free(p->p_username);
    free(p->p_cwdpath);
    memset(p, 0, sizeof(struct peer));
}