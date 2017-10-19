#ifndef HANDLER_H
#define HANDLER_H

#include "server/handler/peer/peer.h"

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
handler_new(int sfd);

peer_t
handler_getcurrent();

peer_t
handler_gettotal();

void
handler_delete_first_if(int (*predicate)(struct peer* ppeer));

void
handler_delete_all_if(int (*predicate)(struct peer* ppeer));

void
handler_foreach(void (*consumer)(struct peer* ppeer));

int
handler_find_first_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer));

int
handler_fina_all_and_apply(int (*predicate)(struct peer* ppeer),
        void (*consumer)(struct peer* ppeer));

int
handler_perform(struct peer* subj, void (*consumer)(struct peer* p));

#endif
