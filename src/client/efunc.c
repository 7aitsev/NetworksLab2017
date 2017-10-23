#include "efunc.h"

#include <errno.h>
#ifdef __linux__
#include <sys/socket.h>
#include <sys/types.h>
#else
#define MSG_NOSIGNAL 0
#define EMSGSIZE WSAEMSGSIZE
#endif

int
readcrlf(SOCKET sfd, char *buf, size_t bsize)
{
    char *bufx = buf;
    size_t len = bsize;
    int rc;
    char c;
    char lastc = 0;

    while(len > 0)
    {
        if(1 != (rc = recv(sfd, &c, 1, 0)))
        {
            if(0 > rc && EINTR == errno)
                continue;
            return rc;
        }
        if ('\n' == c)
        {
            if('\r' == lastc)
                buf--;
            *buf = '\0';
            return buf - bufx + 1;
        }

        *buf++ = c;
        lastc = c;
        len--;
    }
    errno = EMSGSIZE;
    return -1;
}

int
readn(SOCKET sfd, char *buf, size_t bsize)
{
    int cnt;
    int rc;
    int len = bsize;

    cnt = len;
    while(0 < cnt)
    {
        rc = recv(sfd, buf, cnt, 0);
        if(0 > rc)
        {
            if(EINTR == errno)
                continue;
            return -1;
        }
        if(0 == rc)
            return len - cnt;
        buf += rc;
        cnt -= rc;
    }
    return len;
}

int
sendall(SOCKET sfd, const char* buf, size_t* bsize)
{
    int total = 0;
    int bytesleft = *bsize;
    int n;

    while((size_t) total < *bsize)
    {
        n = send(sfd, buf + total, bytesleft, MSG_NOSIGNAL);
        if(-1 == n)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *bsize = total;

    return n == -1 ? -1 : 0;
}
