#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/peer/peer.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#define HANDLER_PEERS_LIMIT 25
#define HANDLER_PEERS_SIZE 5

static peer_t g_current;
static peer_t g_total;

static int g_peerslen;
static struct peer* g_peers;

static pthread_mutex_t g_lock;

void
handler_init()
{
    logger_log("[handler] initializing...\n");
    g_peerslen = HANDLER_PEERS_SIZE;
    g_peers = malloc(g_peerslen * sizeof(struct peer));
    memset(g_peers, 0, sizeof(g_peerslen * sizeof(struct peer)));
    pthread_mutex_init(&g_lock, NULL);
}

void
handler_destroy()
{
    logger_log("[handler] destroing...\n");
    free(g_peers);
    pthread_mutex_destroy(&g_lock);
}

int
handler_getcurrent()
{
    int current;
    pthread_mutex_lock(&g_lock);
    current = g_current;
    pthread_mutex_unlock(&g_lock);
    return current;
}

int
handler_gettotal()
{
    int total;
    pthread_mutex_lock(&g_lock);
    total = g_total;
    pthread_mutex_unlock(&g_lock);
    return total;
}

void
peerleft()
{
    pthread_mutex_lock(&g_lock);
    --g_current;
    pthread_mutex_unlock(&g_lock);
}

static void*
handler_test(void* arg)
{
    struct peer* ppeer = (struct peer*) arg;
    int sfd = ppeer->p_sfd;
    char buffer[100];

    while(1)
    {
        int n = recv(sfd, buffer, 99, 0);
        buffer[n] = '\0';
        if(n == 0)
        {
            break;
        }

        logger_log("%d, recv:%s\n", sfd, buffer);

        send(sfd, "I got your message", 18, 0);
    }
    peer_shutdown(ppeer);
    pthread_detach(ppeer->p_tid);
    peerleft();
    memset(ppeer, 0, sizeof(struct peer));

    return arg;
}

static void
find_first_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer))
{
    for(int i = 0; i < g_peerslen; ++i)
    {
        if(predicate(g_peers + i))
        {
            consumer(g_peers + i);
            break;
        }
    }
}

static void
find_all_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer))
{
    for(int i = 0; i < g_peerslen; ++i)
    {
        if(predicate(g_peers + i))
        {
            consumer(g_peers + i);
        }
    }
}

static void
delete_peer(struct peer* ppeer)
{
    logger_log("[handler] Deleting the peer: sfd=%d, tid=%d\n",
            ppeer->p_sfd, ppeer->p_tid);
    --g_current;
    pthread_cancel(ppeer->p_tid);
    pthread_join(ppeer->p_tid, NULL);
    peer_shutdown(ppeer);
    memset(ppeer, 0, sizeof(struct peer));
}

void
handler_new(int sfd)
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] new peer sfd=%d\n", sfd);
    find_first_and_apply(
            lambda(int, (struct peer* p) { return 0 == p->p_tid; }),
            lambda(void, (struct peer* p)
                {
                    if(NULL != p)
                    {
                        p->p_sfd = sfd;
                        pthread_create(&p->p_tid, NULL, handler_test, p);
                    }
                })
        );
    pthread_mutex_unlock(&g_lock);
}

void
handler_deletefirst(int (*predicate)(struct peer* ppeer))
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] delete first\n");
    find_first_and_apply(predicate, &delete_peer);
    pthread_mutex_unlock(&g_lock);
}

void
handler_deleteall(int (*predicate)(struct peer* ppeer))
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] delete all\n");
    find_all_and_apply(predicate, &delete_peer);
    pthread_mutex_unlock(&g_lock);
}

void
handler_foreach(void (*cb)(struct peer* p))
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] foreach\n");
    find_all_and_apply(
            lambda(int, (struct peer* p) { return 0 != p->p_tid; }),
            cb
        );
    pthread_mutex_unlock(&g_lock);
}

/*
static void
handler_join_cb(void* arg)
{
    struct peer* p = (struct peer*) arg;
    logger_log("[handler] handler_join_cb\n");
    pthread_join(p->p_tid, NULL);
}*/
