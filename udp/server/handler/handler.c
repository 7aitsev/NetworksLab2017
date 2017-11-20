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

void
handler_init()
{
    logger_log("[handler] initializing...\n");
    g_peerslen = HANDLER_PEERS_SIZE;
    g_peers = malloc(g_peerslen * sizeof(struct peer));
    memset(g_peers, 0, g_peerslen * sizeof(struct peer));
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

struct peer*
get_peer_from_array(struct sockaddr_in* addr)
{
    const struct peer* peers_end = g_peers + g_peerslen;
    for(struct peer* p = g_peers; peers_end != p; ++p)
    {
        if(peer_are_addrs_equal(&p->p_addr, addr))
        {
            return p;
        }
    }
    return NULL;
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

/**
 * -1 - no action required
 *  0 - a response has to be sent
 */
int
handler_new(char* buf, int bufsize, struct sockaddr_storage* addr)
{
    struct peer* _peer;
    struct sockaddr_in* peer_addr;

    if(-1 == peer_check_family(addr))
    {
        logger_log("[server] unsupported adress family: %d\n",
                addr->ss_family);
        return -1;
    }

    peer_addr = (struct sockaddr_in*) addr;
    _peer = get_peer_from_array(peer_addr);
    if(NULL == _peer) // new peer
    {
        logger_log("[handler] adding peer...\n");
        int was_found = handler_find_first_and_apply(
            peer_is_not_exist,
            lambda(void, (struct peer* p)
            {
                ++g_current;
                p->p_id = ++g_total;
                peer_cpy_addr(p, peer_addr);
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

    logger_log("[handler] received \"%s\"\n", buf);
    // a response is going to be in the buffer
    return service(buf, bufsize, _peer); // return how many bytes to send
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