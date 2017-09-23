#ifndef HANDLER_H
#define HANDLER_H

#include "server/peer/peer.h"

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

int
handler_getcurrent();

int
handler_gettotal();

void
handler_deletefirst(int (*predicate)(struct peer* ppeer));

void
handler_deleteall(int (*predicate)(struct peer* ppeer));

void
handler_foreach(void (*consumer)(struct peer* ppeer));

#endif
