#ifndef SERVICE_H
#define SERVICE_H

#include "server/handler/peer/peer.h"

int
service(struct peer* p);

void
service_extend_time(struct peer* p);

int
service_is_peer_expired(struct peer* p);

#endif