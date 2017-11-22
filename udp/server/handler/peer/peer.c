#include "lib/werror.h"
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
                p->p_username, p->p_cwd, p->p_mode);
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
    free(p->p_username); // this also frees p_cwd
    memset(p, 0, sizeof(struct peer));
}

int
peer_check_order(struct peer* p, unsigned int seq)
{
    return (p->p_seq < seq) ? 0 : -1;
}

int
peer_relative_path(struct peer* p, const char* path, char** resolved)
{
    int resolved_path_size;
    char* buf = NULL;

    if('\\' != path[0] && ':' != path[1])
    {
        int from_str_size =
                strlen(p->p_cwd) + strlen(path) + strlen("\\0");
        buf = malloc(from_str_size);
        sprintf(buf, "%s\\%s", p->p_cwd, path);
        path = buf;
    }
    
    resolved_path_size = GetFullPathName(path, 0, NULL, NULL);
    if(! resolved_path_size)
    {
        logger_log("[peer] GetFullPathName failed for \"%s\": %s\n",
                path, wstrerror());
        free(buf);
        return resolved_path_size;
    }

    *resolved = malloc(resolved_path_size);
    GetFullPathName(path, resolved_path_size, *resolved, NULL);

    logger_log("[peer] resolved path=%s\n", *resolved);

    free(buf);
    return resolved_path_size;
}