#include "lib/efunc.h"
#include "lib/termproto.h"
#include "logger/logger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char * const TERM_METHOD_STRING[] = {
    "AUTH", "LS", "KILL", "WHO", "LOGOUT"
};

const char * const TERM_STATUS_ALL[] = {
    "200", "OK",
    "400", "Bad Request",
    "403", "Forbidden",
    "404", "Not Found",
    "500", "Internal Server Error",
};

int
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
fill_term_req(struct TERM_REQ* term_req, const char* method, char* path)
{
    int rv;
    
    if(-1 == (rv = isstrin(method, TERM_METHOD_STRING, HEAD)))
    {
        term_req->status = BAD_REQUEST;
        return -1;
    }
    term_req->method = rv;

    memset(term_req->path, 0, TERMPROTO_PATH_SIZE);
    strncpy(term_req->path, path, TERMPROTO_PATH_SIZE);
    if(term_req->path[TERMPROTO_PATH_SIZE - 1] != '\0')
    {
        // path is too long, but the request itself is ok
        term_req->status = INTERNAL_ERROR;
        term_req->path[TERMPROTO_PATH_SIZE - 1] = '\0';
        return -1;
    }
    free(path);

    return 0;
}

int
parse_term_req(struct TERM_REQ* term_req, const char* req)
{
    char method[8];
    char* path = NULL;

    int rv;
    errno = 0;
    if(2 == (rv = sscanf(req, "%7[A-Z] %ms", method, &path)))
    {
        return fill_term_req(term_req, method, path);
    }
    else if(0 < rv)
    {
        logger_log("[termproto] Not all values were matched\n");
        free(path);
        term_req->status = BAD_REQUEST;
    }
    else if(0 == rv && 0 == errno)
    {
        logger_log("[termproto] No matching values\n");
        term_req->status = BAD_REQUEST;
    }
    else
    {
        logger_log("[termproto] sscanf failed: %d %s\n", rv, strerror(errno));
        term_req->status = INTERNAL_ERROR;
    }
    return -1;
}

void
error_term(int sfd, struct TERM_REQ* req)
{
    char resp[32];
    size_t size;

    //size = put_term_header(resp, req, NULL);

    sendall(sfd, resp, &size);
}

void
do_term_resp(int sfd, struct TERM_REQ* req)
{
    size_t size = 28;
    printf("method=%s\tpath=%s\n",
            TERM_METHOD_STRING[req->method], req->path);
    sendall(sfd, "200 OK\r\n\r15\nhello/\nworld.txt", &size);
}

void
term_mk_resp(int sfd, const char* data)
{
    struct TERM_REQ treq;
    
    if(0 == parse_term_req(&treq, data))
    {
        do_term_resp(sfd, &treq);
    }
    /*else
    {
        error_term(sfd, &treq);
    }*/
}
