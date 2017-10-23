#ifndef EFUNC_H
#define EFUNC_H

#ifdef __WIN32__
#include <winsock2.h>
#else
typedef SOCKET int
#endif
#include <stddef.h>

int
readcrlf(SOCKET sfd, char *buf, size_t bsize);

int
readn(SOCKET sfd, char *buf, size_t bsize);

int
sendall(SOCKET sfd, const char* buf, size_t* bsize);

#endif
