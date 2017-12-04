#include "lib/efunc.h"
#include "lib/termproto.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>

static SOCKET g_sfd;
static struct term_req g_req;
static char g_running = 0;
static const size_t g_bufsize = TERMPROTO_BUF_SIZE;
static char g_buf[TERMPROTO_BUF_SIZE];

static int prompt_len;
static char PROMPT[300];
static char g_username[11];

void
set_prompt(const char* cwd)
{
    if(NULL != cwd)
        prompt_len = snprintf(PROMPT, 300, "%s:%s$ ",
                g_username, cwd);
}

void
print_prompt()
{
    fputs(PROMPT, stdout);
}

void
error(const char *err_msg, const SOCKET *socket, void (*exit)(int))
{
    static char buf[1024];
    if(0 != WSAGetLastError())
    {
        if(0 == FormatMessage( // 0 means failure
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(), 0, buf, 1024, NULL))
        {
            fprintf(stderr, "FormatMessage() failed: err=0x%lx\n", GetLastError());
        }
        else
        {
            fprintf(stderr, "%s:\n%s\n", err_msg, buf);
        }
    }
    else
    {
        fprintf(stderr, "%s\n", err_msg);
    }

    if(socket != NULL)
    {
        shutdown(*socket, SD_BOTH);
        closesocket(*socket);
    }

    if(exit != NULL)
    {
        WSACleanup();
        (*exit)(EXIT_FAILURE);
    }
}

void
prepareclient(char* host, char* port)
{
    SOCKET sfd = INVALID_SOCKET;
    int yes = 1;
    struct addrinfo hints;
    struct addrinfo* p;
    struct addrinfo* serverInfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if(0 != getaddrinfo(host, port, &hints, &serverInfo))
    {
        error("getpeername() failed", NULL, exit);
    }

    for(p = serverInfo; NULL != p; p = p->ai_next)
    {
        sfd = socket( serverInfo->ai_family, serverInfo->ai_socktype,
                serverInfo->ai_protocol);
        if(sfd == INVALID_SOCKET)
        {
            continue;
        }

        if(0 != setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, 
                (char*) &yes, sizeof(yes)))
        {
            error("setsockopt() failed", NULL, exit);
        }

        if(0 != connect(sfd, p->ai_addr, p->ai_addrlen))
        {
            error("connect() failed", &sfd, NULL);
            sfd = INVALID_SOCKET;
            continue;
        }
        break;
    }
    freeaddrinfo(serverInfo);

    if(sfd == INVALID_SOCKET)
    {
        error("Unable to connect to the server!", NULL, exit);
    }

    g_sfd = sfd;
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
isempty(const char *s) {
  while('\0' != *s) {
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
getpass(const char *prompt, char* password, unsigned char psize)
{  
    DWORD dwRead;
    DWORD con_mode;
    int overlim = 0;
    unsigned char ch;
    unsigned char plen = 0;
    unsigned char plim = psize - 1;

    fputs(prompt, stdout);

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    GetConsoleMode(hIn, &con_mode);
    SetConsoleMode(hIn, con_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

    while(ReadConsoleA(hIn, &ch, 1, &dwRead, NULL) && '\r' != ch)
    {
        if(plen == plim)
        {
            if('\b' != ch)
                ++overlim;
            else
            {
                --overlim;
                if(-1 == overlim)
                {
                    overlim = 0;
                    --plen;
                }
            }
        }
        else if('\b' == ch)
        {
            if(plen != 0)
            {
                --plen;
            }
        }
        else
        {
            password[plen++] = ch;
        }
    }
    password[plen] = '\0';
    SetConsoleMode(hIn, con_mode);
    putchar('\n');

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
    while(0 == getpass("Password: ", pass, credsize))
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
                term_get_method(g_req.method),
                term_get_status_desc(g_req.status));
    }
}

void
send_req()
{
    size_t n;

    n = term_mk_req_header(&g_req, g_buf, g_bufsize);
    if(-1 == sendall(g_sfd, g_buf, &n))
        error("sendall() failed", NULL, exit);
}

int
read_resp_line()
{
    int rv;

    while(1)
    {
        rv = readcrlf(g_sfd, g_buf, g_bufsize);
        if(0 < rv)
        {
            return --rv;
        }
        else if(0 == rv)
        {
            error("server shut down", NULL, exit);
        }
        else
        {
            error("readcrlf() failed", NULL, exit);
        }
    }
    return rv;
}

void
seek_to_resp_body()
{
    if(0 != read_resp_line(g_sfd, g_buf, g_bufsize))
    {
        error("The response could not be read: bad format", NULL, exit);
    }
}

void
print_resp_body(msgsize_t size)
{
    int rv;
    size_t left, toread;

    left = size;
    while(left > 0)
    {
        toread = (left < g_bufsize) ? left : g_bufsize - 1;
        rv = readn(g_sfd, g_buf, toread);
        if(rv > 0)
        {
            left -= rv;
            g_buf[rv] = '\0';
            fputs(g_buf, stdout);
        }
        else if(rv == 0)
        {
            error("Server shut down", NULL, exit);
        }
        else
        {
            error("readn() failed", NULL, exit);
        }
    }
    putchar('\n');
}

void
cp_resp_body(msgsize_t size)
{
    int rv = readn(g_sfd, g_buf, size);
    if(0 < rv)
    {
        char* p = g_buf + rv;
        while(0 < rv--)
        {
            if('\n' == *(--p))
            {
                if(0 < rv && '\r' == *(p - 1))
                    --p;
                *p = '\0';
            }
        }
    }
    else if(rv == 0)
    {
        error("Server shut down", NULL, exit);
    }
    else
    {
        error("readn() failed", NULL, exit);
    }
}

void
recv_resp()
{
    int resp_body_size;

    if(0 == read_resp_line())
        error("A response header was expected", NULL, exit);

    if(0 <= (resp_body_size = term_parse_resp_status(&g_req, g_buf)))
    {
        if(0 < resp_body_size)
        {
            seek_to_resp_body();
        }

        if(OK == g_req.status)
        {
            switch(g_req.method)
            {
                case CD:
                    cp_resp_body(resp_body_size);
                    set_prompt(g_buf);
                    break;
                case AUTH:
                case LS:
                case WHO:
                    print_resp_body(resp_body_size);
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
                print_resp_body(resp_body_size);
            }
        }
    }
    else
    {
        fprintf(stderr, "Received bad response.\n");
        exit(-1);
    }
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
        error("scanf() failed", NULL, NULL);
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

    g_running = 1;
    setvbuf(stdout, NULL, _IONBF, 0);

    authenticate();

    while(g_running)
    {
        print_prompt();
        cmdlen = read_cmd(cmdbuf, CMDBUFSIZE);
        if(0 == cmdlen)
            continue;
        if(-1 == parse_cmd(cmdbuf))
            continue;

        handle_cmd();
    }

    shutdown(g_sfd, SD_BOTH);
    closesocket(g_sfd);

    return;
}

int
main(int argc, char** argv)
{
    if(3 != argc)
    {
        fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    WSADATA wsaData;
    if(0 != WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        error("WSAStartup() failed", NULL, exit);
    }

    prepareclient(argv[1], argv[2]);
    runclient();

    WSACleanup();

    return 0;
}
