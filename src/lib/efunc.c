#include "lib/efunc.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

int
readcrlf(int s, char *buf, size_t len)
{
    char *bufx = buf;
    int rc;
    char c;
    char lastc = 0;

    while(len > 0)
    {
        if(1 != (rc = recv(s, &c, 1, 0)))
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
            return buf - bufx;
        }

        *buf++ = c;
        lastc = c;
        len--;
    }
    errno = EMSGSIZE;
    return -1;
}

int
readn(int fd, char *bp, size_t len)
{
    int cnt;
    int rc;

    cnt = len;
    while(0 < cnt)
    {
        rc = recv(fd, bp, cnt, 0);
        if(0 > rc)
        {
            if(EINTR == errno)
                continue;
            return -1;
        }
        if(0 == rc)
            return len - cnt;
        bp += rc;
        cnt -= rc;
    }
    return len;
}

int
sendall(int sfd, const char* data, size_t* dsize)
{
    int total = 0;
    int bytesleft = *dsize;
    int n;

    while((size_t) total < *dsize)
    {
        n = send(sfd, data+total, bytesleft, MSG_NOSIGNAL);
        if(-1 == n)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *dsize = total;

    return n == -1 ? -1 : 0;
}
