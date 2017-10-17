#ifndef EFUNC_H
#define EFUNC_H

#include <stddef.h>

int
readcrlf(int s, char *buf, size_t len);

int
readn(int fd, char *bp, size_t len);

int
sendall(int sfd, const char* data, size_t* dsize);

#endif
