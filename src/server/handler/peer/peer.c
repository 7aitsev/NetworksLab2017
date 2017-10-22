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

void
peer_printinfo(struct peer* p)
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof addr;

    getpeername(p->p_sfd, (struct sockaddr*) &addr, &len);

    if(AF_INET == addr.ss_family)
    {
        int port;
        char mode = peer_get_mode(p);
        char ipstr[INET_ADDRSTRLEN];

        struct sockaddr_in *s = (struct sockaddr_in*) &addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

        printf("Peer #%d\n\tIP address: %s\n\tPort: %d\n\t"
                "Socket: %d\n",
                p->p_id, ipstr, port, p->p_sfd);
        if(PEER_NO_PERMS != mode)
        {
            printf("\tUsername: %s\n\tMode: %d\n",
                    p->p_username, mode);
        }
        else
        {
            printf("\tNot authorised\n");
        }
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
    if(STDIN_FILENO != p->p_cwd)
        close(p->p_cwd);
    free(p->p_username);
    free(p->p_buffer);
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

static inline int
peer_get_fdcwd(struct peer* p)
{
    return __sync_fetch_and_or(&p->p_cwd, 0);
}

char*
peer_get_cwd(struct peer* p, char* rpath, size_t rplen)
{
    char path[rplen];
    int fdcwd = peer_get_fdcwd(p);

    sprintf(path, "/proc/self/fd/%d", fdcwd);
    memset(rpath, 0, rplen);
    if(-1 != readlink(path, rpath, rplen - 1))
    {
        return rpath;
    }
    logger_log("[peer] readlink error: %s\n", strerror(errno));
    return NULL;
}

/**
 * Invokes only from the <service> module which is the only one
 * who can modify cwd. So it is safe to read p->p_cwd from the module,
 * but not to modify, because other threads may be reading at the same
 * moment. Other threads have to use thread-safety methods to read p_cwd.
 */
int
peer_set_cwd(struct peer* p, const char* path)
{
    int dirfd;

    if(STDIN_FILENO == p->p_cwd)
    {
        dirfd = open(path, O_RDONLY);
        if(-1 == dirfd)
        {
            return -1;
        }
        __sync_add_and_fetch(&p->p_cwd, dirfd);
    }
    else
    {
        struct stat path_stat;

        dirfd = openat(p->p_cwd, path, O_RDONLY);
        if(-1 == dirfd)
        {
            return -1;
        }
        
        fstat(dirfd, &path_stat);
        if(!S_ISDIR(path_stat.st_mode))
        {
            errno = ENOTDIR;
            return -1;
        }

        dup2(dirfd, p->p_cwd); // assume this doesn't fail
        close(dirfd);
    }
    return 0;
}
