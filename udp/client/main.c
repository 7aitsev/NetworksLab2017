#include "lib/termproto.h"

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static int g_sfd;
static unsigned short int g_seq;
static struct term_req g_req;
static char g_running = 0;
static int g_len;
static const int g_bufsize = TERMPROTO_BUF_SIZE;
static char g_buf[TERMPROTO_BUF_SIZE];

static int prompt_len;
static char PROMPT[300];
static char g_username[11];

void
set_prompt(const char* cwd)
{
    if(NULL != cwd)
        prompt_len = snprintf(PROMPT, 300, "%s#%s$ ",
                g_username, cwd);
}

void
print_prompt()
{
    fputs(PROMPT, stdout);
}

void
error(const char *err_msg, const int socket, void (*exit)(int))
{
    if(0 != errno)
    {
        fprintf(stderr, "\n\n%s: %s\n", err_msg, strerror(errno));
    }
    else
    {
        fprintf(stderr, "\n\n%s\n", err_msg);
    }

    if(0 != socket)
    {
        close(socket);
    }

    if(NULL != exit)
    {
        (*exit)(EXIT_FAILURE);
    }
}

void
prepareclient(char* host, char* port)
{
    int yes = 1;
    struct addrinfo hints;
    struct addrinfo* p;
    struct addrinfo* serv_info;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if(0 != getaddrinfo(host, port, &hints, &serv_info))
    {
        error("getpeername() failed", 0, exit);
    }

    for(p = serv_info; NULL != p; p = p->ai_next)
    {
        g_sfd = socket(serv_info->ai_family, serv_info->ai_socktype,
                serv_info->ai_protocol);
        if(-1 == g_sfd)
        {
            continue;
        }

        if(-1 == setsockopt(g_sfd, SOL_SOCKET, SO_REUSEADDR, 
                (char*) &yes, sizeof(yes)))
        {
            error("setsockopt() failed", 0, exit);
        }

        if(0 != connect(g_sfd, p->ai_addr, p->ai_addrlen))
        {
            error("connect() failed", g_sfd, NULL);
            g_sfd = -1;
            continue;
        }

        break;
    }
    freeaddrinfo(serv_info);

    if(-1 == g_sfd)
    {
        error("Unable to connect to the server!", 0, exit);
    }
}

char*
cmd_toupper(char * cmd)
{
    char *s = cmd;
    while(*s)
    {
        *s = toupper(*s);
        s++;
    }
    return cmd;
}

int
isempty(const char *s)
{
    while('\0' != *s)
    {
        if(! isspace((unsigned char) *s))
            return 0;
        s++;
    }
    return 1;
}

int
validated_or_get_len(char* cred)
{
    char* c = cred;
    while('\0' != *c)
    {
        if(';' == *c++)
            return 0;
    }
    return cred - c;
}

int
getuname(const char* prompt, char* uname)
{
    char c, rv;

    fputs(prompt, stdout);
    rv = scanf("%10s", uname);

    while('\n' != (c = getchar()) && EOF != c);

    if(rv != 1)
    {
        return 0;
    }

    return validated_or_get_len(uname);
}

int
getpasswd(const char *prompt, char* password, unsigned char psize)
{
    char* inp_pass;
    inp_pass = getpass(prompt);
    strncpy(password, inp_pass, psize - 1);
    password[psize - 1] = '\0';

    return validated_or_get_len(password);
}

void
mk_auth_req()
{
    unsigned char credsize = 11;
    char uname[credsize];
    char pass[credsize];

    while(0 == getuname("Username: ", uname))
    {
        fprintf(stderr, "Bad username. Try again\n");
    }
    while(0 == getpasswd("Password: ", pass, credsize))
    {
        fprintf(stderr, "Bad password. Try again\n");
    }

    g_req.method = AUTH;
    snprintf(g_req.path, TERMPROTO_PATH_SIZE, "%s;%s",
            uname, pass);
    strncpy(g_username, uname, 11);
}

void
print_bad_resp()
{
    if(OK != g_req.status)
    {
        fprintf(stderr, "%s: %s\n",
                term_get_method_str(g_req.method),
                term_get_status_str(g_req.status));
    }
}

void
send_req()
{
    size_t n;

    g_req.seq = ++g_seq;
    n = term_mk_req_header(&g_req, g_buf, g_bufsize);
    if(-1 == send(g_sfd, g_buf, n, MSG_NOSIGNAL))
    {
        error("sendto() failed", 0, exit);
    }
}

void
print_resp_body()
{
    if(NULL != g_req.msg)
        puts(g_req.msg);
    else
        putchar('\n');
}

void
recv_resp()
{
    int rv;

    g_len = recv(g_sfd, g_buf, g_bufsize, 0);
    if(-1 == g_len)
    {
        error("recv failed", g_sfd, exit);
    }
    else if(0 == g_len)
    {
        return;
    }
    g_buf[(g_bufsize == g_len) ? g_len - 1 : g_len] = '\0';

    if(0 == (rv = term_parse_resp_status(&g_req, g_buf)))
    {
        if(g_req.seq != g_seq)
        {
            puts("Received unordered message");
            return;
        }

        if(OK == g_req.status)
        {
            switch(g_req.method)
            {
                case CD:
                    set_prompt(g_req.msg);
                    break;
                case AUTH:
                case LS:
                case WHO:
                    print_resp_body();
                    break;
                case LOGOUT:
                    g_running = 0;
                    break;
                default:
                    break;
            }
        }
        else
        {
            print_bad_resp();
            if(AUTH == g_req.method && FORBIDDEN == g_req.status)
            {
                print_resp_body();
            }
        }
    }
    else
    {
        error("Received bad response", 0, NULL);
    }
    print_prompt();
}

void
handle_cmd()
{
    send_req();

    recv_resp();
}

int
parse_cmd(const char* buf)
{
    int rv;
    char method[8];
    g_req.path[0] = '\0';

    errno = 0;
    rv = sscanf(buf, "%7s %255[^\r\n]", method, g_req.path);
    if(0 < rv)
    {
        int cmd;
        if(-1 != (cmd = term_is_valid_method(cmd_toupper(method))))
        {
            g_req.method = cmd;
            switch(cmd)
            {
                case AUTH:
                    printf("You are already logged in\n");
                    return -1;
                case CD:
                case LS:
                case WHO:
                    if(2 != rv)
                        strcpy(g_req.path, ".");
                    break;
                case LOGOUT:
                    strncpy(g_req.path, g_username, 11);
                    break;
                case KILL:
                    if(2 != rv)
                    {
                        fprintf(stderr, "Usage: kill username\n");
                        return -1;
                    }
            }
            return 0;
        }
        else
        {
            fprintf(stderr, "Unknown command: %s\n", method);
            return -1;
        }
    }
    else if(0 == errno)
    {
        fprintf(stderr, "No matching values\n");
        return -1;
    }
    else
    {
        fprintf(stderr, "scanf() failed");
        return -1;
    }
}

int
read_cmd(char* buf, int bufsize)
{
    int buflen;
    *buf = '\0';

    if(NULL == fgets(buf, bufsize, stdin))
    {
        perror("fgets() failed while reading stdin");
        return -1;
    }
    if(isempty(buf))
    {
        return 0;
    }
    buflen = strnlen(buf, bufsize);
    if(buflen == bufsize)
    {
        buf[--buflen] = '\0';
    }
    buf[buflen] = '\0';
    return buflen;
}

void
authenticate()
{
    do
    {
        mk_auth_req();
        handle_cmd();
    } while(OK != g_req.status);
    g_req.method = CD;
    strcpy(g_req.path, ".");
    handle_cmd();
}

void
runclient()
{
    size_t cmdlen;
    enum {CMDBUFSIZE = 300};
    char cmdbuf[CMDBUFSIZE];
    fd_set allfd;
    fd_set readfd;
    struct timeval tv;
    unsigned char heartbeats = 0;
    unsigned char disable_input = 0;
    unsigned char received_hb = 0;

    authenticate();

    FD_ZERO(&allfd);
    FD_SET(STDIN_FILENO, &allfd);
    FD_SET(g_sfd, &allfd);
    tv.tv_sec = TERMPROTO_T1;
    tv.tv_usec = 0;

    g_running = 1;
    while(g_running)
    {
        readfd = allfd;
        int rc = select(g_sfd + 1, &readfd, NULL, NULL, &tv);
        if(rc > 0)
        {
            if(FD_ISSET(STDIN_FILENO, &readfd))
            {

                cmdlen = read_cmd(cmdbuf, CMDBUFSIZE);
                if(0 == cmdlen || -1 == parse_cmd(cmdbuf))
                {
                    print_prompt();
                    continue;
                }
                FD_CLR(STDIN_FILENO, &allfd);
                disable_input = 1;
                received_hb = 0;
                send_req();
            }
            else if(FD_ISSET(g_sfd, &readfd))
            {
                recv_resp();
                heartbeats = 0;
                tv.tv_sec = TERMPROTO_T1;

                if(0 != g_len && g_seq != g_req.seq)
                    continue;
                if(0 == g_len && 0 == disable_input)
                    continue;
                if(0 == g_len)
                {
                    if(3 < ++received_hb)
                        puts("Response timeout");
                    else
                        continue;
                }

                FD_SET(STDIN_FILENO, &allfd);
                disable_input = 0;
                received_hb = 0;
            }
            else
            {
                error("select returned invalid socket", g_sfd, exit);
            }
        }
        else if(0 == rc)
        {
            if(++heartbeats > 3)
                error("Connection dead", g_sfd, exit);
            if(-1 == send(g_sfd, NULL, 0, 0))
                error("send failure", g_sfd, exit);
            tv.tv_sec = TERMPROTO_T2;
        }
        else
        {
            error("select failure", g_sfd, exit);
        }
    }

    close(g_sfd);
}

int
main(int argc, char** argv)
{
    if(3 != argc)
    {
        fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
        return 1;
    }

    prepareclient(argv[1], argv[2]);

    setvbuf(stdout, NULL, _IONBF, 0);

    runclient();

    return 0;
}
