#include "lib/termproto.h"
#include "logger/logger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char * const TERM_METHOD_STRING[] = {
    "AUTH", "LS", "CD", "KILL", "WHO", "LOGOUT"
};

const char * const TERM_STATUS_ALL[] = {
    "200", "OK",
    "400", "Bad Request",
    "403", "Forbidden",
    "404", "Not Found",
    "405", "Not a Directory",
    "408", "Request Timeout",
    "500", "Internal Server Error",
};

static int
isstrin(const char* str, const char * const set[], size_t latest_el)
{
    size_t i;

    for(i = 0; ; ++i)
    {
        if(0 == strcmp(str, set[i]))
        {
            return i;
        }
        else if(i == latest_el)
        {
            return -1;
        }
    }
}

static int
fill_term_req(struct term_req* req, const char* method, char* path)
{
    int rv;
    
    if(-1 == (rv = isstrin(method, TERM_METHOD_STRING, LOGOUT)))
    {
        req->status = BAD_REQUEST;
        return -1;
    }
    req->method = rv;

    memset(req->path, 0, TERMPROTO_PATH_SIZE);
    strncpy(req->path, path, TERMPROTO_PATH_SIZE);
    if(req->path[TERMPROTO_PATH_SIZE - 1] != '\0')
    {
        // path is too long, but the request itself is ok
        req->status = INTERNAL_ERROR;
        req->path[TERMPROTO_PATH_SIZE - 1] = '\0';
        return -1;
    }
    free(path);

    return 0;
}

int
term_parse_req(struct term_req* req, const char* buf)
{
    char method[8];
    char* path = NULL;

    int rv;
    errno = 0;
    if(2 == (rv = sscanf(buf, "%7[A-Z] %ms", method, &path)))
    {
        return fill_term_req(req, method, path);
    }
    else if(0 < rv)
    {
        logger_log("[termproto] Not all values were matched\n");
        free(path);
        req->status = BAD_REQUEST;
    }
    else if(0 == errno)
    {
        logger_log("[termproto] No matching values\n");
        req->status = BAD_REQUEST;
    }
    else
    {
        logger_log("[termproto] sscanf failed: %s\n", strerror(errno));
        req->status = INTERNAL_ERROR;
    }
    return -1;
}

size_t
term_put_header(char* buf, size_t bufsize, enum TERM_STATUS status,
        size_t size)
{
    size_t n;
    n = snprintf(buf, bufsize, "%s %s\r\n", TERM_STATUS_ALL[status],
            TERM_STATUS_ALL[status + 1]);
    if(0 < size && n < bufsize)
    {
        n += snprintf(buf + n, bufsize - n, "LENGTH: %ld\r\n\r\n", size);
    }
    return (n < bufsize) ? n : bufsize - 1;
}
