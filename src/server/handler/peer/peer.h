#ifndef PEER_H
#define PEER_H

#include <pthread.h>

#define PEER_NO_PERMS 0
#define PEER_REGULAR 1
#define PEER_SUPER 2

typedef unsigned short int peer_t;

struct peer
{
    peer_t p_id;
    pthread_t p_tid;
    int p_sfd;
    char* p_buffer;
    size_t p_buflen;

    /* could be modified from multiple threads */
    int p_port;
    unsigned int p_ip; // struct in_addr
    char* p_username;
    char p_mode;
    int p_cwd;
};
    
void
peer_printinfo(struct peer* p);

void
peer_destroy(struct peer* p);

int
peer_isexist(struct peer* p);

int
peer_isnotexist(struct peer* p);

void
peer_closesocket(int sfd);

void
peer_handle(struct peer* p);

char
peer_get_mode(struct peer* p);

void
peer_set_mode(struct peer* p, char mode);

int
peer_get_fdcwd(struct peer* p);

char*
peer_get_cwd(struct peer* p, char* rpath, size_t rplen);

int
peer_set_cwd(struct peer* p, const char* path);

#endif
