#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/service/service.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <winsock2.h>

#define HANDLER_PEERS_SIZE 20

static peer_t g_current;
static peer_t g_total;

static peer_t g_peerslen;
static struct peer* g_peers;

/* from "service" module */
extern LARGE_INTEGER g_frequency;
extern struct peer* g_peer;

void
handler_init()
{
    logger_log("[handler] initializing...\n");
    g_peerslen = HANDLER_PEERS_SIZE;
    g_peers = malloc(g_peerslen * sizeof(struct peer));
    memset(g_peers, 0, g_peerslen * sizeof(struct peer));
    QueryPerformanceFrequency(&g_frequency);
}

void
handler_destroy()
{
    logger_log("[handler] destroing...\n");
    handler_delete_all_if(peer_is_exist);
    free(g_peers);
}

peer_t
handler_getcurrent()
{
    return g_current;
}

peer_t
handler_gettotal()
{
    return g_total;
}

static int
get_peer_from_array(struct sockaddr_storage* addr, struct peer** out_peer)
{
    if(-1 == peer_check_family(addr))
    {
        logger_log("[server] unsupported adress family: %d\n",
                addr->ss_family);
        return -1;
    }

    const struct peer* peers_end = g_peers + g_peerslen;
    for(struct peer* p = g_peers; peers_end != p; ++p)
    {
        if(peer_are_addrs_equal(&p->p_addr, (struct sockaddr_in*) addr))
        {
            *out_peer = p;
            return 1;
        }
    }
    return 0;
}

int
handler_find_first_and_apply(int (*predicate)(struct peer* ppeer),
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

int
handler_find_all_and_apply(int (*predicate)(struct peer* ppeer),
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

int
handler_new_request(struct sockaddr_storage* addr)
{
    struct peer* _peer;

    int rv = get_peer_from_array(addr, &_peer);
    if(0 == rv) // new peer
    {
        logger_log("[handler] adding peer...\n");
        int was_found = handler_find_first_and_apply(
            peer_is_not_exist,
            lambda(void, (struct peer* p)
            {
                ++g_current;
                p->p_id = ++g_total;
                memcpy(&p->p_addr, (struct sockaddr_in*) addr,
                    sizeof(struct sockaddr_in));
                logger_log("[handler] peer was added to the array\n");
                _peer = p;
            })
        );

        if(! was_found)
        {
            logger_log("[handler] reached the peers limit\n");
            return -1;
        }
    }
    else if(-1 == rv)
    {
        return rv;
    }

    // a response is going to be in the buffer
    return service(_peer); // return how many bytes to send
}

int
handler_touch_peer(struct sockaddr_storage* addr)
{
    struct peer* _peer;

    if(get_peer_from_array(addr, &_peer))
    {
        service_extend_time(_peer);
        return 0;
    }
    else return -1;
}

static void
deletepeer(struct peer* p)
{
    if(0 == peer_touch_cache(p))
        logger_log("[handler] Deleting the peer #%d: ip=%s, "
                "port=%i\n", p->p_id, p->p_ipstr, p->p_port);

    --g_current;
    peer_destroy(p);
}

void
handler_remove_expired()
{
    handler_delete_all_if(service_is_peer_expired);
}

int
handler_delete_first_if(int (*predicate)(struct peer* ppeer))
{
    return handler_find_first_and_apply(predicate, deletepeer);
}

int
handler_delete_all_if(int (*predicate)(struct peer* ppeer))
{
    return handler_find_all_and_apply(predicate, deletepeer);
}

void
handler_foreach(void (*cb)(struct peer* p))
{
    handler_find_all_and_apply(peer_is_exist, cb);
}