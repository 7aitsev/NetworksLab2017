#include "logger/logger.h"
#include "server/handler/peer/peer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * This function uses printf(), because it has to print details
 * to stdout on a request from the <terminal> module.
 */
void
peer_printinfo(struct peer* p)
{
    int port = p->p_port;
    unsigned int ip = p->p_ip;
    char mode = peer_get_mode(p);
    char ipstr[INET_ADDRSTRLEN];

    if(0 == port)
    {
        int rv;
        struct sockaddr_storage addr;
        socklen_t len = sizeof addr;

        logger_log("[peer] no cache for Peer#%hd\n", p->p_id);
        rv = getpeername(p->p_sfd, (struct sockaddr*) &addr, &len);
        if(-1 == rv)
        {
            logger_log("[peer] getpeername failed for Peer#%hd\n",
                    p->p_id);
            return;
        }

        if(AF_INET == addr.ss_family)
        {
            struct sockaddr_in *s = (struct sockaddr_in*) &addr;
            port = ntohs(s->sin_port); 
            ip = (unsigned int) s->sin_addr.s_addr;

            p->p_port = port;
            p->p_ip = ip;
        }
        else
        {
            printf("Peer#%d has unsupported adress family\n",
                    p->p_id);
            return;
        }
    }
    inet_ntop(AF_INET, &ip, ipstr, sizeof ipstr);

    printf("Peer #%d\n\tIP address: %s\n\tPort: %d\n\t"
            "Socket: %d\n",
            p->p_id, ipstr, port, p->p_sfd);
    if(PEER_NO_PERMS != mode)
    {
        printf("\tUsername: %s\n\tCWD: %s\n\tMode: %d\n",
                p->p_username, p->p_cwdpath, mode);
    }
    else
    {
        printf("\tNot authorised\n");
    }
}

void
peer_destroy(struct peer* p)
{
    peer_closesocket(p->p_sfd);
    if(STDIN_FILENO != p->p_cwd)
        close(p->p_cwd);
    free(p->p_username);
    free(p->p_buffer);
    free(p->p_cwdpath);
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

char
peer_get_mode(struct peer* p)
{
    return __sync_or_and_fetch(&p->p_mode, 0);
}

void
peer_set_mode(struct peer* p, char mode)
{
    __sync_add_and_fetch(&p->p_mode, mode);
}

int
peer_set_cwd(struct peer* p, const char* path, int psize)
{
    int dirfd;
    struct stat path_stat;

    char* resolved = realpath(path, NULL); // allocates memory
    if(NULL == resolved)
    {
        logger_log("[peer] realpath failed for: path=%s\n", path);
        return -1;
    }
    resolved[psize - 1] = '\0';

    dirfd = open(resolved, O_RDONLY);
    if(-1 == dirfd)
    {
        logger_log("[peer] truncated?\npath=%s\n", resolved);
        free(resolved);
        return -1;
    }

    fstat(dirfd, &path_stat);
    if(!S_ISDIR(path_stat.st_mode))
    {
        errno = ENOTDIR;
        close(dirfd);
        free(resolved);
        return -1;
    }

    free(p->p_cwdpath);
    p->p_cwdpath = resolved;

    if(STDIN_FILENO != p->p_cwd)
    {
        dup2(dirfd, p->p_cwd); // assume this doesn't fail
        close(dirfd);
    }
    else
    {
        p->p_cwd = dirfd;
    }
    return 0;
}
