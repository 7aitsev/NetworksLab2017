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

    /* could be accessed from multiple threads */
    char* p_username;
    char p_mode;
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

#endif
