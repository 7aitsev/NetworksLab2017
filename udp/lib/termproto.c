#include "termproto.h"
#include "logger/logger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define METHOD_SIZE 8

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
    "500", "Internal Server Error"
};

char*
term_get_method_str(int method)
{
    return (char*) TERM_METHOD_STRING[method];
}

char*
term_get_status_str(int status)
{
    return (char*) TERM_STATUS_ALL[status + 1];
}

static int
find_str_idx(const char* str, const char * const set[], int latest_el)
{
    int i;

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
    return find_str_idx(method, TERM_METHOD_STRING, LOGOUT);
}

int
term_is_valid_status(const char* status)
{
    return find_str_idx(status, TERM_STATUS_ALL, INTERNAL_ERROR);
}

int
term_parse_req(struct term_req* req, const char* buf)
{
    char method[METHOD_SIZE];

    errno = 0;
    req->seq = 0;
    int rv = sscanf(buf, "%u %7[A-Z] %255[^\r\n]",
                    &req->seq, method, req->path);
    if(3 == rv)
    {
        rv = term_is_valid_method(method);
        if(-1 == rv)
        {
            req->status = BAD_REQUEST;
            return -1;
        }
        req->method = rv;
        return 0;
    }
    else if(0 < rv)
    {
        logger_log("[termproto] Not all values were matched\n");
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

int
term_put_header(char* buf, int bufsize, unsigned int seq,
    enum TERM_STATUS status)
{
    int n;
    n = snprintf(buf, bufsize, "%u %s %s\r\n", seq,
        TERM_STATUS_ALL[status], TERM_STATUS_ALL[status + 1]);
    return (n < bufsize) ? n : bufsize;
}
/*
size_t
term_mk_req_header(struct term_req* req, char* buf, size_t bufsize)
{
    size_t n;
    n = snprintf(buf, bufsize, "%s %s\r\n",
            TERM_METHOD_STRING[req->method], req->path);
    return (n < bufsize) ? n : bufsize;
}

int
term_parse_resp_status(struct term_req* req, char* buf)
{
    int rv;
    char status[4];
    char status_txt[22];
    status_txt[0] = '\0';
    
    rv = sscanf(buf, "%3s %21s", status, status_txt);
    if(2 <= rv)
    {
        int s = term_is_valid_status(status);
        if(-1 == s)
            return s;
        req->status = s;
        strncpy(req->path, status_txt, 22);
    }
    else
    {
        return -1;
    }
    return 0;
}

int
term_parse_resp_body(char* buf)
{
    int rv;
    int len;
    
    rv = sscanf(buf, "LENGTH: %d", &len);
    return (1 == rv) ? len : -1;
}*/