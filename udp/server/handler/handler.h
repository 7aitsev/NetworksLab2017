#ifndef HANDLER_H
#define HANDLER_H

#include <ws2tcpip.h>

#include "server/handler/peer/peer.h"

#define HANDLER_BUFSIZE 1024

#define lambda(return_type, function_body) \
({ \
      return_type __fn__ function_body \
          __fn__; \
})

void
handler_init();

void
handler_destroy();

void
handler_new(char* buf, int bufsize, struct sockaddr_storage* addr);

peer_t
handler_getcurrent();

peer_t
handler_gettotal();
/*
int
handler_delete_first_if(int (*predicate)(struct peer* ppeer));
*/
int
handler_delete_all_if(int (*predicate)(struct peer* ppeer));

void
handler_foreach(void (*consumer)(struct peer* ppeer));

int
handler_find_first_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer));

int
handler_find_all_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer));
/*
void
handler_perform(struct peer* subj, void (*consumer)(struct peer* p));
*/
#endif