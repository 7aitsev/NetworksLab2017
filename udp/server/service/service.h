#ifndef SERVICE_H
#define SERVICE_H

#include "server/handler/peer/peer.h"

int
service(char* buf, int bufsize, struct peer* p);

#endif