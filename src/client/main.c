#include "efunc.h"
#include "termproto.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#define EMPTY_MSG ""

static char g_running = 0;
static const size_t g_bufsize = TERMPROTO_BUF_SIZE;
static char g_buf[TERMPROTO_BUF_SIZE];

int prompt_len;
char PROMPT[300];
char g_username[11];

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
    char *dec_err_code = NULL;
    if(0 != WSAGetLastError())
    {
        if(0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                0, (DWORD) WSAGetLastError(), 0, (LPSTR) &dec_err_code, 0, 0))
        {
            *dec_err_code = '\0';
        }
        fprintf(stderr, "%s:\n%s\n", err_msg, dec_err_code);
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

SOCKET
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

    return sfd;
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

    return strlen(uname);
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
                    password[--plen] = '\0';
                }
            }
        }
        else if('\b' == ch)
        {
            if(plen != 0)
            {
                password[--plen] = '\0';
            }
        }
        else
        {
            password[plen++] = ch;
        }
    }
    SetConsoleMode(hIn, con_mode);
    putchar('\n');

    return strlen(password);
}

void
mk_auth_req(struct term_req* req)
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

    snprintf(req->path, TERMPROTO_PATH_SIZE, "%s;%s",
            uname, pass);
    strncpy(g_username, uname, 11);
}

int
send_req(SOCKET sfd, struct term_req* req)
{
    size_t n;

    n = term_mk_req_header(req, g_buf, g_bufsize);
    return sendall(sfd, g_buf, &n);
}

int
wait_resp(SOCKET sfd, struct term_req* req)
{
    int rv;
    size_t dlen  = 0;

    rv = send_req(sfd, req);
    if(-1 == rv)
    {
        error("sendall() failed", &sfd, exit);
        req->status = INTERNAL_ERROR;
    }

    rv = recv(sfd, g_buf, g_bufsize, 0);
    if(0 == rv)
    {
        fprintf(stderr, "server shut down\n");
        g_running = 0;
        return -1;
    }
    else if(-1 == rv)
    {
        error("recv failed", NULL, NULL);
        req->status = INTERNAL_ERROR;
        return -1;
    }

    //write(1, g_buf, rv);
    dlen = rv;
    if(0 == (rv = term_parse_resp(req, g_buf, &dlen)))
    {
        //printf("raw=%s\n, dlen=%d\nmsg=%s\n", g_buf, dlen, req->msg);
        return dlen;
    }
    return rv;
}

void
print_bad_resp(struct term_req* req)
{
    if(OK != req->status)
    {
        fprintf(stderr, "%s: %s\n",
                term_get_method(req->method),
                term_get_status_desc(req->status));
    }
}

void
print_msg(struct term_req* req)
{
    if(0 != strcmp(EMPTY_MSG, req->msg))
    {
        printf("%s\n", req->msg);
    }
}

void
handle_cmd(SOCKET sfd, struct term_req* req)
{
    int rv = 0;
    req->status = OK;
    req->msg = EMPTY_MSG;

    switch(req->method)
    {
        case AUTH:
            do
            {
                print_bad_resp(req);
                print_msg(req);
                mk_auth_req(req);
            }
            while(-1 != (rv = wait_resp(sfd, req))
                && OK != req->status);
            if(OK == req->status)
                printf("You are logged in\n");
            return;
        case LS:
            if(-1 != wait_resp(sfd, req) && req->status != OK)
            {
                print_msg(req);
            }
            else
            {
                print_bad_resp(req);
            }
            break;
        case CD:
            if(-1 != wait_resp(sfd, req))
            {
                if(OK == req->status)
                {
                    set_prompt(req->msg);
                }
                else
                {
                    print_bad_resp(req);
                }
            }
            break;
        case LOGOUT:
            strncpy(req->path, g_username, 11);
            if(-1 != wait_resp(sfd, req))
            {
                g_running = 0;
            }
            else
            {
                print_bad_resp(req);
            }
            break;
        default:
            fprintf(stderr, "Command not implemented\n");
            break;
    }
}

int
parse_cmd(struct term_req* req, const char* buf)
{
    int rv;
    char method[8];
    req->path[0] = '\0';

    errno = 0;
    rv = sscanf(buf, "%7s %255s\r\n", method, req->path);
    if(0 < rv)
    {
        int cmd;
        if(-1 != (cmd = term_is_valid_method(cmd_toupper(method))))
        {
            req->method = cmd;
            if(2 != rv)
            {
                strcpy(req->path, " .");
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
recv_resp(SOCKET sfd, char* buf, size_t bufsize)
{
    int rv;
    int offset = 0;
    //struct term_req;
    
    while(1)
    {
        rv = readcrlf(sfd, buf + offset, bufsize - offset);
        if(0 < rv)
        {
            // first line: 200_OK\r\nLENGTH:_0
            offset += --rv;
            
        }
        else if(0 == rv)
        {
            // server shutdown the connections
        }
        else
        {
            // error
        }
    }
}

void
runclient(SOCKET sfd)
{
    size_t cmdlen;
    enum {CMDBUFSIZE = 300};
    char cmdbuf[CMDBUFSIZE];
    struct term_req req;

    g_running = 1;
    setvbuf(stdout, NULL, _IONBF, 0);

    {
        req.method = term_is_valid_method("AUTH");
        handle_cmd(sfd, &req);
        if(0 == g_running)
            return;
        req.method = term_is_valid_method("CD");
        strcpy(req.path, ".");
        handle_cmd(sfd, &req);
        if(0 == g_running)
            return;
    }

    while(g_running)
    {
        print_prompt();
        cmdlen = read_cmd(cmdbuf, CMDBUFSIZE);
        if(0 == cmdlen)
            continue;
        if(-1 == parse_cmd(&req, cmdbuf))
            continue;
        if(AUTH == req.method)
        {
            printf("You are already logged in\n");
            req.method = LS;
            continue;
        }

        handle_cmd(sfd, &req);
    }

    shutdown(sfd, SD_BOTH);
    closesocket(sfd);

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
    
    SOCKET sfd = prepareclient(argv[1], argv[2]);
    runclient(sfd);
    
    WSACleanup();
    
    return 0;
}