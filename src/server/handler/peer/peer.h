#ifndef PEER_H
#define PEER_H

#include <pthread.h>

typedef unsigned short int peer_t;

struct peer
{
    peer_t p_id;
    pthread_t p_tid;
    int p_sfd;
};
    
void
peer_printinfo(struct peer* p);

void
peer_set(struct peer* to, const struct peer* from);

void
peer_setargs(struct peer* p,
        const peer_t id, const pthread_t tid, const int sfd);

void
peer_destroy(struct peer* p);

int
peer_isexist(struct peer* p);

int
peer_isnotexist(struct peer* p);

void
peer_closesocket(int sfd);

#endif
