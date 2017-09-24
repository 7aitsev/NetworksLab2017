#include "logger/logger.h"
#include "server/handler/handler.h"

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
    handler_deleteall_if(&peer_isexist);
    free(g_peers);
    pthread_mutex_destroy(&g_lock);
}

int
handler_getcurrent()
{
    return __sync_or_and_fetch(&g_current, 0);
}

int
handler_gettotal()
{
    return __sync_or_and_fetch(&g_total, 0);
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

        logger_log("[handler] %d, recv:%s\n", sfd, buffer);

        send(sfd, "I got your message", 18, 0);
    }
    pthread_detach(ppeer->p_tid);
    peer_destroy(ppeer);
    __sync_sub_and_fetch(&g_current, 1);

    return arg;
}

static int
find_first_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer))
{
    const struct peer* peers_end = g_peerslen + g_peers;
    for(struct peer* p = g_peers; peers_end != p; ++p)
    {
        if(predicate(p))
        {
            consumer(p);
            return 1;
        }
    }
    return 0;
}

static int
find_all_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer))
{
    int wasfound = 0;
    const struct peer* peers_end = g_peerslen + g_peers;
    for(struct peer* p = g_peers; peers_end != p; ++p)
    {
        if(predicate(p))
        {
            wasfound = 1;
            consumer(p);
        }
    }
    return wasfound;
}

static void
deletepeer(struct peer* ppeer)
{
    logger_log("[handler] Deleting the peer #%d: sfd=%d, tid=%u\n",
            ppeer->p_id, ppeer->p_sfd, ppeer->p_tid);
    __sync_sub_and_fetch(&g_current, 1);
    pthread_cancel(ppeer->p_tid);
    pthread_join(ppeer->p_tid, NULL);
    peer_destroy(ppeer);
}

void
handler_new(int sfd)
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] new peer sfd=%d\n", sfd);
    int isfound = find_first_and_apply(
            &peer_isnotexist,
            lambda(void, (struct peer* p)
                {
                    p->p_sfd = sfd;
                    p->p_id = __sync_add_and_fetch(&g_total, 1);
                    __sync_add_and_fetch(&g_current, 1);
                    pthread_create(&p->p_tid, NULL, handler_test, p);
                })
        );
    if(!isfound)
    {
        logger_log("[handler] Reached the peers limit\n");
        peer_closesocket(sfd);
    }
    pthread_mutex_unlock(&g_lock);
}

void
handler_deletefirst_if(int (*predicate)(struct peer* ppeer))
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] delete first\n");
    find_first_and_apply(predicate, &deletepeer);
    pthread_mutex_unlock(&g_lock);
}

void
handler_deleteall_if(int (*predicate)(struct peer* ppeer))
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] delete all\n");
    find_all_and_apply(predicate, &deletepeer);
    pthread_mutex_unlock(&g_lock);
}

void
handler_foreach(void (*cb)(struct peer* p))
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] foreach\n");
    find_all_and_apply(&peer_isexist, cb);
    pthread_mutex_unlock(&g_lock);
}
