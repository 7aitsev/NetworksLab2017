#include "termproto.h"

#ifdef __linux__
#include "logger/logger.h"
#else
    void logger_log(char* phony, ...) { };
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char * const TERM_METHOD_STRING[] = {
    "AUTH", "LS", "CD", "KILL", "WHO", "LOGOUT"
};

static const char * const TERM_STATUS_ALL[] = {
    "200", "OK",
    "400", "Bad Request",
    "403", "Forbidden",
    "404", "Not Found",
    "405", "Not a Directory",
    "408", "Request Timeout",
    "500", "Internal Server Error",
};

char*
term_get_method(int method)
{
    return TERM_METHOD_STRING[method];
}

char*
term_get_status_desc(int status)
{
    return TERM_STATUS_ALL[status + 1];
}

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

int
term_is_valid_method(const char* method)
{
    return isstrin(method, TERM_METHOD_STRING, LOGOUT);
}

int
term_is_valid_status(const char* status)
{
    return isstrin(status, TERM_STATUS_ALL, INTERNAL_ERROR);
}

static int
fill_term_req(struct term_req* req, const char* method, char* path)
{
    int rv;
    
    if(-1 == (rv = term_is_valid_method(method)))
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
    if(2 == (rv = sscanf(buf, "%7[A-Z] %255ms", method, &path)))
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
        n += snprintf(buf + n, bufsize - n, "LENGTH: %d\r\n\r\n", size);
    }
    return (n < bufsize) ? n : bufsize;
}

size_t
term_mk_req_header(struct term_req* req, char* buf, size_t bufsize)
{
    size_t n;
    n = snprintf(buf, bufsize, "%s %s\r\n",
            TERM_METHOD_STRING[req->method], req->path);
    return (n < bufsize) ? n : bufsize;
}

char*
getpostion(char* buf, size_t len)
{
    int cnt = 0;
    char* pos = NULL;
    
    if(len == TERMPROTO_BUF_SIZE)
        len--;
    buf[len] = '\0';

    while(NULL != (pos = strtok(buf, "\n")))
    {
        if(++cnt > 2 && '\r' != *pos)
        {
            cnt = strlen(pos);
            if('\r' == pos[cnt - 1])
                pos[cnt - 1] = '\0';
            return pos;
        }
    }
    return pos;
}

int
term_parse_resp(struct term_req* req, char* buf, size_t* len)
{
    int rv;
    size_t dlen = 0;
    char status[4];
    char status_txt[22];
    status_txt[0] = '\0';
    
    rv = sscanf(buf, "%3s %21sLENGTH: %d", status, status_txt, &dlen);
    if(2 <= rv)
    {
        int s = term_is_valid_status(status);
        if(-1 == s)
            return s;
        req->status = s;
        strncpy(req->path, status_txt, 22);
        if(3 == rv && 0 != dlen)
        {
            req->msg = getpostion(buf, rv);
            *len = dlen;
        }
    }
    else
    {
        return -1;
    }
    return 0;
}