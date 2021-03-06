#include "lib/efunc.h"
#include "lib/termproto.h"
#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/service/service.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_PATH "/"
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
    msgsize_t bodylen = strlen(req->msg);
    bodylen += (bodylen != 0) ? 2 : 0;

    respsize = term_put_header(p->p_buffer, p->p_buflen, req->status,
            bodylen);
    if(MSG_EMPTY != req->msg)
        respsize += sprintf(p->p_buffer + respsize, "%s\r\n", req->msg);
    sendall(p->p_sfd, p->p_buffer, &respsize);
}

static char*
get_username(struct peer* p, char* login)
{
    handler_perform(p, lambda(void, (struct peer* pp)
    {
        login = pp->p_username;
    }));
    return login;
}

static int
check_username(const char* inp, const char* login)
{
    return (NULL != strstr(inp, login)) ? 1 : 0;
}

static int
isitpeer(struct peer* p, char* inp)
{
    char* login = NULL;

    login = get_username(p, login);
    return check_username(inp, login);
}

static int
find_in_db(FILE* fdb, const char* login, const char* pass)
{
    char l[11];
    char p[11];
    char m;

    while(3 == fscanf(fdb, " %10[a-zA-Z] %10[^;\t\r\n ] %hhd", l, p, &m))
    {
        if(0 == strcmp(login, l) && 0 == strcmp(pass, p))
            return m;
    }

    return PEER_NO_PERMS;
}

static void
do_auth(struct peer* p, struct term_req* req)
{
    if(PEER_NO_PERMS == peer_get_mode(p))
    {
        char login[11];
        char pass[11];
        int rv;

        rv = sscanf(req->path, "%10[a-zA-Z];%10s", login, pass);
        if(2 == rv)
        {
            FILE* db = fopen(DB_ACCOUNTS, "r");
            if(NULL != db)
            {
                rv = find_in_db(db, login, pass);
                if(rv != 0)
                {
                    peer_set_mode(p, rv);
                    handler_perform(p, lambda(void, (struct peer* pp)
                    {
                        pp->p_username = malloc(11);

                        peer_set_cwd(p, DEFAULT_PATH, 0);
                        strcpy(p->p_username, login);
                    }));

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
                fclose(db);
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
count_names_len(int fdcwd, struct term_req* req)
{
    int cnt = 0;
    struct dirent* entry;
    DIR* root;
    int dirfd = openat(fdcwd, req->path, O_RDONLY);

    req->status = OK;
    if(-1 == dirfd)
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

    root = fdopendir(dirfd);
    if(NULL == root)
    {
        if(ENOTDIR == errno)
        {
            req->status = NOT_DIR;
        }
        close(dirfd);
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
    size_t n = 0;
    msgsize_t bs = p->p_buflen;
    char* buf = p->p_buffer;
    DIR* root;
    int fdcwd;

    handler_perform(p, lambda(void, (struct peer* pp)
    {
        fdcwd = pp->p_cwd;
    }));

    int cnt = count_names_len(fdcwd, req);
    if(0 > cnt)
    {
        error_term(p->p_sfd, req);
        logger_log("[service] cant read a dir: %s\n", strerror(errno));
        return;
    }
    
    n = term_put_header(buf, bs, req->status, cnt);
    if(0 < cnt)
    {
        msgsize_t prev;
        struct dirent* entry;

        root = fdopendir(openat(fdcwd, req->path, O_RDONLY));
        while(NULL != (entry = readdir(root)))
        {
            if(entry->d_name[0] != '.')
            {
                prev = n;
                n += snprintf(buf + n, bs - n, "%s%s\r\n", entry->d_name,
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
        closedir(root);
    }

    sendall(p->p_sfd, buf, &n);
}

static void
do_cd(struct peer* p, struct term_req* req)
{
    int rv;
    handler_perform(p, lambda(void, (struct peer* pp)
    {
        rv = peer_set_cwd(pp, req->path, TERMPROTO_PATH_SIZE);
    }));
    if(0 == rv)
    {
        char* path;
        handler_perform(p, lambda(void, (struct peer* pp)
        {
            path = pp->p_cwdpath;
        }));
        req->status = OK;
        req->msg = path;
        small_resp(p, req);
        logger_log("[service] chdir=%s\n", path);
    }
    else
    {
        switch(errno)
        {
            case EACCES:
                req->status = FORBIDDEN;
                break;
            case ENOENT:
                req->status = NOT_FOUND;
                break;
            case ENOTDIR:
                req->status = NOT_DIR;
                break;
            default:
                req->status = INTERNAL_ERROR;
        }
        logger_log("[service] chdir failed: %s\n",
                strerror(errno));
        error_term(p->p_sfd, req);
    }
}

static void
do_kill(struct peer* p, struct term_req* req)
{
    int rv;

    if(! isitpeer(p, req->path))
    {
        rv = handler_delete_all_if(lambda(int, (struct peer* predic)
        {
            if(NULL != predic->p_username)
                return NULL != strstr(req->path, predic->p_username);
            else
                return 0;
        }));

        req->status = (rv == 1) ? OK : NOT_FOUND;
    }
    else
    {
        req->status = FORBIDDEN;
    }
    small_resp(p, req);
}

static void
do_who(struct peer* p, struct term_req* req)
{
    msgsize_t n;
    msgsize_t offset;
    size_t tocpy;
    size_t tosend;
    int peers_cnt = 0;
    int bsize = 6 * TERMPROTO_BUF_SIZE; // big enough for 20 peers
    char* buf = malloc(bsize);

    if(NULL == buf)
    {
        logger_log("[service] who: malloc failed\n");
        req->status = INTERNAL_ERROR;
        error_term(p->p_sfd, req);
        return;
    }

    offset = sprintf(buf, "ID\tUNAME\tMODE\tCWD\n");
    handler_foreach(lambda(void, (struct peer* pp)
    {
        char mode = peer_get_mode(pp);
        if(0 != mode)
        {
            ++peers_cnt;
            offset += sprintf(buf + offset, "%d\t%s\t%d\t%s\n",
                    pp->p_id, pp->p_username, mode,
                    pp->p_cwdpath);
        }
    }));
    offset += sprintf(buf + offset, "TOTAL: %d\n", peers_cnt);

    req->status = OK;
    n = term_put_header(p->p_buffer, p->p_buflen, req->status, offset);
    if(n + offset <= p->p_buflen)
    {
        tocpy = offset;
        strncpy(p->p_buffer + n, buf, tocpy);
        tosend = tocpy + n;
        sendall(p->p_sfd, p->p_buffer, &tosend);
    }
    else
    {
        tocpy = p->p_buflen - n;
        strncpy(p->p_buffer + n, buf, tocpy);
        tosend = p->p_buflen;
        sendall(p->p_sfd, p->p_buffer, &tosend);
        tocpy = offset - p->p_buflen;
        sendall(p->p_sfd, buf + p->p_buflen, &tocpy);
    }
    
    free(buf);
}

static void
do_logout(struct peer* p, struct term_req* req)
{
    if(isitpeer(p, req->path))
    {
        logger_log("[service] logout: username=%s\n", req->path);
        req->status = OK;
    }
    else
    {
        req->status = BAD_REQUEST;
    }
    small_resp(p, req);
}

static int
handle_req(struct peer* p)
{
    int rv;
    struct term_req req;

    rv = term_parse_req(&req, p->p_buffer);
    if(0 == rv)
    {
        req.msg = MSG_EMPTY;
        int methiskill = (req.method == KILL);

        if(PEER_SUPER == peer_get_mode(p) && methiskill)
        {
            do_kill(p, &req);
        }
        else if(PEER_NO_PERMS < peer_get_mode(p) && !methiskill)
        {
            switch(req.method)
            {
                case AUTH:
                    do_auth(p, &req);
                    break;
                case CD:
                    do_cd(p, &req);
                    break;
                case LS:
                    do_ls(p, &req);
                    break;
                case WHO:
                    do_who(p, &req);
                    break;
                case LOGOUT:
                    do_logout(p, &req);
                    if(OK == req.status)
                        return 1;
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
    return 0;
}

void
service(struct peer* p)
{
    int sfd = p->p_sfd;
    size_t len = TERMPROTO_BUF_SIZE;
    char* buffer = malloc(len);

    if(NULL != buffer)
    {
        p->p_buffer = buffer;
        p->p_buflen = len;

        while(1)
        {
            int rv = readcrlf(sfd, buffer, len);
            if(0 < rv)
            {
                rv = handle_req(p);
                if(1 == rv)
                    return;
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
}
