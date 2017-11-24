#ifndef PEER_H
#define PEER_H

#include <ws2tcpip.h>

#define PEER_NO_PERMS 0
#define PEER_REGULAR 1
#define PEER_SUPER 2

#define SOCK_ADDR_IN_PORT(sin) ((sin)->sin_port)
#define SOCK_ADDR_IN_ADDR(sin) ((sin)->sin_addr.s_addr)
#define PEER_PORT(p) ((p)->p_addr.sin_port)
#define PEER_ADDR(p) ((p)->p_addr.sin_addr.s_addr)

typedef unsigned short int peer_t;

struct peer
{
    peer_t p_id;
    struct sockaddr_in p_addr;
    
    /* cached parameters */
    char p_ipstr[INET_ADDRSTRLEN];
    unsigned short int p_port;
    long long p_time;

    peer_t p_seq;
    char* p_username; // null-terminated
    char p_mode;
    char* p_cwd; // null-terminated
};

int
peer_touch_cache(struct peer* p);

void
peer_printinfo(struct peer* p);

int
peer_check_family(struct sockaddr_storage* addr);

int
peer_are_addrs_equal(struct sockaddr_in* a, struct sockaddr_in* b);

int
peer_is_exist(struct peer* p);

int
peer_is_not_exist(struct peer* p);

void
peer_destroy(struct peer* p);

int
peer_check_order(struct peer* p, unsigned int seq);

int
peer_relative_path(struct peer* p, const char* path, char** resolved);

#endif