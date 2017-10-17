#include "lib/efunc.h"
#include "lib/termproto.h"
#include "logger/logger.h"
#include "server/handler/handler.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#define HANDLER_PEERS_LIMIT 20
#define HANDLER_PEERS_SIZE 5

static peer_t g_current;
static peer_t g_total;

static peer_t g_peerslen;
static struct peer* g_peers;
#define DOUBLE_LENGTH ((g_peerslen << 1) >= HANDLER_PEERS_LIMIT \
                                          ? HANDLER_PEERS_LIMIT \
                                          : g_peerslen << 1)

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
    handler_delete_all_if(&peer_isexist);
    free(g_peers);
    pthread_mutex_destroy(&g_lock);
}

peer_t
handler_getcurrent()
{
    return __sync_or_and_fetch(&g_current, 0);
}

peer_t
handler_gettotal()
{
    return __sync_or_and_fetch(&g_total, 0);
}

static void*
handler_test(void* arg)
{
    pthread_mutex_lock(&g_lock);
    struct peer p;
    struct peer* ppeer = (struct peer*) arg;
    if(peer_isnotexist(ppeer))
    {
        // somehow the peer had been destroyed
        //  before the thread started
        pthread_mutex_unlock(&g_lock);
        return arg;
    }
    peer_set(&p, ppeer);
    pthread_mutex_unlock(&g_lock);

    int sfd = p.p_sfd;
    size_t len = TERMPROTO_BUF_SIZE;
    char* buffer = malloc(len);

    if(NULL != buffer)
    {
        while(1)
        {
            memset(buffer, 0, len);
            int rv = readcrlf(sfd, buffer, len);
            if(0 < rv)
            {
                term_mk_resp(sfd, buffer);
            }
            else if(0 == rv)
            {
                logger_log("[handler] client %hd hung up\n", p.p_id);
                break;
            }
            else
            {
                logger_log("[handler] readcrlf: %s\n", strerror(errno));
                break;
            }
        }
    }
    else
    {
        logger_log("[handler] malloc failed: %s\n",
                strerror(errno));
    }
    
    free(buffer);

    pthread_detach(p.p_tid);
    handler_find_first_and_apply(
            lambda(int, (struct peer* predic)
                {return p.p_id == predic->p_id;}
            ),
            lambda(void, (struct peer* pp)
                {
                    logger_log("[test] Deleting #%d: sfd=%d, tid=%u\n",
                            pp->p_id, pp->p_sfd, pp->p_tid);
                    __sync_sub_and_fetch(&g_current, 1);
                    peer_destroy(pp);
                }
            ));
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

static int
handler_realloc(peer_t newlen)
{
    logger_log("[realloc] newlen=%d\n", newlen);
    struct peer* newpeers;
    int actualsize = newlen * sizeof(struct peer);
    if(NULL != (newpeers = realloc(g_peers, actualsize)))
    {
        g_peers = newpeers;
        if(newlen > g_peerslen)
            memset(g_peers + g_peerslen, 0,
                    sizeof(struct peer) * (newlen - g_peerslen));
        g_peerslen = newlen;
        return 0;
    }
    else
    {
        logger_log("[handler] realloc failed: %s\n", strerror(errno));
        return -1;
    }
}

static void
handler_shrink()
{
    peer_t curr = handler_getcurrent();
    if(g_peerslen > HANDLER_PEERS_SIZE && curr <= g_peerslen / 4)
    {
        size_t cnt = 0;
        for(int i = g_peerslen - 1; cnt != g_peerslen / 2; --i)
        {
            if(peer_isnotexist(g_peers + i))
                ++cnt;
        }
        if(g_peerslen / 2 == cnt)
        {
            logger_log("[handler] shrink from %d to %d\n", g_peerslen, cnt);
            handler_realloc(cnt);
        }
    }
}

void
handler_new(int sfd)
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] new peer sfd=%d\n", sfd);
    handler_shrink();
    while(1)
    {
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
        if(isfound)
        {
            break;
        }
        else
        {
            if(g_peerslen < HANDLER_PEERS_LIMIT)
            {
                logger_log("[handler] realloc from %d to %d\n",
                        g_peerslen, 2 * g_peerslen);
                if(0 == handler_realloc(DOUBLE_LENGTH))
                    continue;
            }
            logger_log("[handler] Reached the peers limit\n");
            peer_closesocket(sfd);
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);
}

void
handler_delete_first_if(int (*predicate)(struct peer* ppeer))
{
    pthread_mutex_lock(&g_lock);
    logger_log("[handler] delete first\n");
    find_first_and_apply(predicate, &deletepeer);
    pthread_mutex_unlock(&g_lock);
}

void
handler_delete_all_if(int (*predicate)(struct peer* ppeer))
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

int
handler_find_first_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer))
{
    int rv;
    pthread_mutex_lock(&g_lock);
    rv = find_first_and_apply(predicate, consumer);
    pthread_mutex_unlock(&g_lock);
    return rv;
}


int
handler_fina_all_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer))
{
    int rv;
    pthread_mutex_lock(&g_lock);
    rv = find_all_and_apply(predicate, consumer);
    pthread_mutex_unlock(&g_lock);
    return rv;
}
