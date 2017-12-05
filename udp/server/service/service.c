#include "lib/werror.h"
#include "lib/termproto.h"
#include "logger/logger.h"
#include "server/handler/handler.h"
#include "server/service/service.h"

#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_PATH "C:\\"
#define DB_ACCOUNTS "%TMP%\\accounts"

static const char * const MSG_EMPTY = "";
static const char * const AUTH_MULTIPLE = "You\'ve been authorised";
static const char * const AUTH_BAD_TRY = "Unable to log in";
static const char * const AUTH_GRANTED = "Successful authentication";

char* g_buf;
const int g_bufsize = TERMPROTO_BUF_SIZE;
const int g_period  = (1000 * (TERMPROTO_T1 + TERMPROTO_T2));
struct peer* g_peer;
LARGE_INTEGER g_frequency;

static int g_bytes_to_send;
static struct term_req g_req;

static void
error_term()
{
    g_bytes_to_send = 
        term_put_header(g_buf, g_bufsize, 0, g_req.status);
}

static void
small_resp()
{
    int respsize;

    respsize = term_put_header(g_buf, g_bufsize, g_peer->p_seq,
        g_req.status);
    if(MSG_EMPTY != g_req.msg)
    {
        respsize += sprintf(g_buf + respsize, "\r\n%s", g_req.msg);
    }
    g_bytes_to_send = respsize;
}

static int
find_in_db(FILE* fdb, const char* login, const char* pass)
{
    char mode;
    char l[11];
    char p[11];

    while(3 == fscanf(fdb, " %10[a-zA-Z] %10[^;\t\r\n ] %c", l, p, &mode))
    {
        if(0 == strcmp(login, l) && 0 == strcmp(pass, p))
            return mode - '0';
    }

    return PEER_NO_PERMS;
}

static void
do_auth()
{
    if(PEER_NO_PERMS == g_peer->p_mode)
    {
        int rv;
        char login[11];
        char pass[11];

        rv = sscanf(g_req.path, "%10[a-zA-Z];%10s", login, pass);
        if(2 == rv)
        {
            char dbpath[TERMPROTO_PATH_SIZE];
            ExpandEnvironmentStrings(DB_ACCOUNTS, dbpath,
                TERMPROTO_PATH_SIZE);
            FILE* db = fopen(dbpath, "r");
            if(NULL != db)
            {
                rv = find_in_db(db, login, pass);
                if(PEER_NO_PERMS != rv)
                {
                    g_peer->p_mode = rv;

                    g_peer->p_username = malloc(11 + TERMPROTO_PATH_SIZE);
                    strcpy(g_peer->p_username, login);

                    g_peer->p_cwd = &g_peer->p_username[11];
                    strcpy(g_peer->p_cwd, DEFAULT_PATH);

                    g_req.status = OK;
                    g_req.msg = AUTH_GRANTED;
                    logger_log("[service] auth: ok\n");
                }
                else
                {
                    g_req.status = FORBIDDEN;
                    g_req.msg = AUTH_BAD_TRY;
                    logger_log("[service] bad login or pass\n");
                }
                fclose(db);
            }
            else
            {
                g_req.status = INTERNAL_ERROR;
                logger_log("[sevice] db error: %s\n", strerror(errno));
            }
        }
        else
        {
            g_req.status = BAD_REQUEST;
            logger_log("[service] login & pass bad format\n");
        }
    }
    else
    {
        g_req.status = OK;
        g_req.msg = AUTH_MULTIPLE;
        logger_log("[service] auth multiple times\n");
    }
    small_resp();
}

DIR*
open_dir(char** newpath, int* newpath_size)
{
    DIR* ret_dir;

    *newpath_size = peer_relative_path(g_peer, g_req.path, newpath);
    if(0 == *newpath_size)
    {
        g_req.status = INTERNAL_ERROR;
        return NULL;
    }

    if(NULL == (ret_dir = opendir(*newpath)))
    {
        switch(GetLastError())
        {
            case ERROR_ACCESS_DENIED:
                g_req.status = FORBIDDEN;
                break;
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                g_req.status = NOT_FOUND;
                break;
            case ERROR_DIRECTORY:
                g_req.status = NOT_DIR;
                break;
            default:
                g_req.status = INTERNAL_ERROR;
        }
        logger_log("[service] opendir failed: %s\n", wstrerror());
    }
    return ret_dir;
}

static void
do_ls()
{
    int n, prev, newpath_size;
    char* newpath;
    struct dirent* entry;

    DIR* dir = open_dir(&newpath, &newpath_size);
    free(newpath);

    if(NULL == dir)
    {
        small_resp();
        return;
    }

    g_req.status = OK;
    n = term_put_header(g_buf, g_bufsize, g_peer->p_seq, g_req.status);
    n += sprintf(g_buf + n, "\r\n");
    while(NULL != (entry = readdir(dir)))
    {
        if(entry->d_name[0] != '.')
        {
            prev = n;
            n += snprintf(g_buf + n, g_bufsize - n, "%s%s\n", entry->d_name,
                    (DT_DIR == entry->d_type) ? "\\" : "");
            if(n >= g_bufsize)
            {
                logger_log("[service] too many files. sizeof(buffer)=%d\n",
                    g_bufsize);
                n = prev; // truncate output
                break;
            }
        }
    }
    closedir(dir);
    g_bytes_to_send = n;
}

static void
do_cd()
{
    char* newpath;
    int newpath_size;
    DIR* dir = open_dir(&newpath, &newpath_size);

    if(NULL != dir)
    {
        g_req.status = OK;
        strcpy(g_peer->p_cwd, newpath);
        g_req.msg = g_peer->p_cwd;

        free(newpath);
        logger_log("[service] chdir=%s\n", g_peer->p_cwd);
    }
    small_resp();
}

static void
do_who()
{
    int n;
    int peers_cnt = 0;

    n = term_put_header(g_buf, g_bufsize, g_peer->p_seq, g_req.status = OK);
    n += sprintf(g_buf + n, "\r\nID\tUNAME\tMODE\tCWD\n");
    handler_foreach(lambda(void, (struct peer* pp)
    {
        if(0 != pp->p_mode)
        {
            ++peers_cnt;
            n += sprintf(g_buf + n, "%d\t%s\t%d\t%s\n",
                    pp->p_id, pp->p_username, pp->p_mode, pp->p_cwd);
        }
    }));
    n += sprintf(g_buf + n, "TOTAL: %d\n", peers_cnt);
    g_bytes_to_send = n;
}

static int
is_same_peer()
{
    return (NULL != strstr(g_req.path, g_peer->p_username)) ? 1 : 0;
}

static void
do_kill()
{
    int rv;

    if(! is_same_peer())
    {
        rv = handler_delete_all_if(lambda(int, (struct peer* pp)
        {
            if(NULL != pp->p_username)
                return NULL != strstr(g_req.path, pp->p_username);
            else
                return 0;
        }));

        g_req.status = (rv == 1) ? OK : NOT_FOUND;
    }
    else
    {
        g_req.status = FORBIDDEN;
    }
    small_resp();
}

static void
do_logout()
{
    if(is_same_peer())
    {
        g_req.status = OK;
        small_resp(); // make a response first, do not delete the peer

        handler_delete_first_if(lambda(int, (struct peer* pp)
        {
            return pp->p_id == g_peer->p_id;
        })); // now it's ok to delete
        logger_log("[service] logout: username=%s\n", g_req.path);
    }
    else
    {
        g_req.status = BAD_REQUEST;
        small_resp();
    }
}

static void
handle_req()
{
    int method_kill = (g_req.method == KILL);

    if(PEER_SUPER == g_peer->p_mode && method_kill)
    {
        do_kill();
    }
    else if(PEER_NO_PERMS < g_peer->p_mode && !method_kill)
    {
        switch(g_req.method)
        {
            case AUTH:
                do_auth();
                break;
            case CD:
                do_cd();
                break;
            case LS:
                do_ls();
                break;
            case WHO:
                do_who();
                break;
            case LOGOUT:
                do_logout();
                break;
            default:
                logger_log("[handler] not implemented\n");
        }
    }
    else if(g_req.method == AUTH)
    {
        do_auth();
    }
    else
    {
        g_req.status = FORBIDDEN;
        small_resp();
    }
}

long long milliseconds_now()
{
    static LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (1000LL * now.QuadPart) / g_frequency.QuadPart;
}

void
service_extend_time(struct peer* p)
{
    p->p_time = milliseconds_now() + g_period;
}

int
service_is_peer_expired(struct peer* p)
{
    return peer_is_exist(p)
            && (milliseconds_now() - p->p_time >= g_period);
}

int
service(struct peer* p)
{
    g_peer = p;
    g_req.msg = MSG_EMPTY;

    int rv = term_parse_req(&g_req, g_buf);

    if(0 != g_req.seq) // successfully parsed seq number
    {
        if(0 == peer_check_order(g_peer, g_req.seq)) // it's a new request
        {
            g_peer->p_seq = g_req.seq;
            if(rv == 0) // request is correct
            {
                handle_req();
            }
            else // we can use seq number to send a bad response
            {
                small_resp();
            }
        }
        else // ignore - whether the request is bad or not
        {
            logger_log("[handler] received unordered request: "
                "peer.seq=%hu, req.seq=%hu\n", g_peer->p_seq, g_req.seq);
            return 0; // server is not going to send 0 bytes
        }
    }
    else
    {
        error_term(); // send error response with seq number = 0
    }

    logger_log("[service] parsed=%d, to send %d\n", rv, g_bytes_to_send);
    service_extend_time(p);
    return g_bytes_to_send;
}
