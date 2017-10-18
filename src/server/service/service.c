#include "lib/efunc.h"
#include "lib/termproto.h"
#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/service/service.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define DB_ACCOUNTS "/tmp/accounts"

static const char * const MSG_EMPTY = "";
static const char * const AUTH_MULTIPLE = "You\'ve been authorised";
static const char * const AUTH_BAD_TRY = "Unable to log in";
static const char * const AUTH_GRANTED = "Successful authentication";

static void
error_term(int sfd, struct term_req* req)
{
    size_t rs = 32;
    char resp[rs];
    size_t size;

    size = term_put_header(resp, rs, req->status, 0);

    sendall(sfd, resp, &size);
}

static void
small_resp(struct peer* p, struct term_req* req)
{
    size_t respsize;
    respsize = term_put_header(p->p_buffer, p->p_buflen, req->status,
            strlen(req->msg));
    if(NULL != req->msg)
        respsize += sprintf(p->p_buffer + respsize, "%s", req->msg);
    sendall(p->p_sfd, p->p_buffer, &respsize);
}

static int
find_in_db(FILE* fdb, const char* login, const char* pass)
{
    char l[11];
    char p[11];
    char m;

    while(3 == fscanf(fdb, "%10s %10s %hhd", l, p, &m))
    {
        if(0 == strcmp(login, l) && 0 == strcmp(pass, p))
            return m;
    }

    return PEER_NO_PERMS;
}

static void
do_auth(struct peer* p, struct term_req* req)
{
    if(PEER_NO_PERMS == p->p_mode)
    {
        char login[11];
        char pass[11];
        int rv;

        rv = sscanf(req->path, "%10[^;];%10s", login, pass);
        if(2 == rv)
        {
            FILE* db = fopen(DB_ACCOUNTS, "r");
            if(NULL != db)
            {
                rv = find_in_db(db, login, pass);
                if(rv != 0)
                {
                    p->p_mode = rv;
                    p->p_username = malloc(11);
                    strcpy(p->p_username, login);

                    req->status = OK;
                    req->msg = AUTH_GRANTED;
                    logger_log("[service] auth: ok\n");
                }
                else
                {
                    req->status = FORBIDDEN;
                    req->msg = AUTH_BAD_TRY;
                    logger_log("[service] bad login or pass\n");
                }
            }
            else
            {
                req->status = INTERNAL_ERROR;
                logger_log("[sevice] db error: %s\n", strerror(errno));
            }
        }
        else
        {
            req->status = BAD_REQUEST;
            logger_log("[service] login & pass bad format\n");
        }
    }
    else
    {
        req->status = OK;
        req->msg = AUTH_MULTIPLE;
        logger_log("[service] auth multiple times\n");
    }
    small_resp(p, req);
}

static int
count_names_len(struct term_req* req)
{
    int cnt = 0;
    struct dirent* entry;
    DIR* root = opendir(req->path);

    req->status = OK;
    if(NULL == root)
    {
        switch(errno)
        {
            case EACCES:
                req->status = FORBIDDEN;
                break;
            case ENOENT:
                req->status = NOT_FOUND;
                break;
            default:
                req->status = INTERNAL_ERROR;
        }
        return -1;
    }

    while(NULL != (entry = readdir(root)))
    {
        if(entry->d_name[0] != '.')
        {
            cnt += strlen(entry->d_name) + 2; // + \r\n
            cnt += (DT_DIR == entry->d_type) ? 1 : 0; // mark /
        }
    }
    closedir(root);

    return cnt;
}

static void
do_ls(struct peer* p, struct term_req* req)
{
    struct dirent* entry;
    DIR* root;

    int cnt = count_names_len(req);
    if(cnt == -1)
    {
        error_term(p->p_sfd, req);
        return;
    }


    size_t n = 0;
    size_t prev;
    char* buf = p->p_buffer;
    size_t bs = p->p_buflen;

    n = term_put_header(buf, bs, req->status, cnt);
    root = opendir(req->path);
    while(NULL != (entry = readdir(root)))
    {
        if(entry->d_name[0] != '.')
        {
            prev = n;
            n += sprintf(buf + n, "%s%s\r\n", entry->d_name,
                    (DT_DIR == entry->d_type) ? "/" : "");
            if(n >= bs)
            {
                size_t tosend = prev;
                sendall(p->p_sfd, buf, &tosend);
                prev = 0;
                n = sprintf(buf, "%s%s\r\n", entry->d_name,
                    (DT_DIR == entry->d_type) ? "/" : "");
            }
        }
    }
    sendall(p->p_sfd, buf, &n);
    closedir(root);
}

static void
handle_req(struct peer* p)
{
    int rv;
    struct term_req req;

    rv = term_parse_req(&req, p->p_buffer);
    if(0 == rv)
    {
        req.msg = MSG_EMPTY;

        /*if(PERMS_SUPER == p->p_mode)
        {
            switch(req.method)
            {
                case KILL:
                    do_kill(p, &req);
            }
        } else */
        if(PEER_NO_PERMS < p->p_mode)
        {
            switch(req.method)
            {
                case AUTH:
                    do_auth(p, &req);
                    break;
                case LS:
                    do_ls(p, &req);
                    break;
                default:
                    logger_log("[handler] not implemented\n");
            }
        }
        else if(req.method == AUTH)
        {
            do_auth(p, &req);
        }
        else
        {
            req.status = FORBIDDEN;
            small_resp(p, &req);
        }
    }
    else
    {
        error_term(p->p_sfd, &req);
    }
}

void
service(struct peer* p)
{
    int sfd = p->p_sfd;
    size_t len = TERMPROTO_BUF_SIZE;
    char* buffer = malloc(len);

    p->p_mode = PEER_NO_PERMS;
    p->p_username = NULL;

    if(NULL != buffer)
    {
        p->p_buffer = buffer;
        p->p_buflen = len;

        while(1)
        {
            memset(buffer, 0, len);
            int rv = readcrlf(sfd, buffer, len);
            if(0 < rv)
            {
                handle_req(p);
            }
            else if(0 == rv)
            {
                logger_log("[handler] peer #%hd hung up\n", p->p_id);
                break;
            }
            else
            {
                logger_log("[handler] readcrlf: %s\n", strerror(errno));
                break;
            }
        }
    }
    else
    {
        logger_log("[peer] malloc failed: %s\n", strerror(errno));
    }
    
    free(buffer);
    free(p->p_username);
}
