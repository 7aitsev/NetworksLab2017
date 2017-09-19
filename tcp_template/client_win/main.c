#include <stdio.h>
#include <stdlib.h>

#include <winsock2.h>
#include <ws2tcpip.h>

void
error(const char *errMsg, const SOCKET *socket, void (*exit)(int))
{
    char *decErrCode = NULL;
    if(0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                          0, (DWORD) WSAGetLastError(), 0, (LPSTR) &decErrCode, 0, 0)
            )
    {
        *decErrCode = '\0';
    }
    fprintf(stderr, ":\n%s %s\n", errMsg, decErrCode);
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

int
readn(SOCKET sfd, char * const data, size_t* dsize)
{
    ssize_t n;
    size_t total = 0;
    size_t bytesleft = *dsize;

    while(total < *dsize)
    {
        n = recv(sfd, data + total, bytesleft, 0);
        if(SOCKET_ERROR == n || 0 == n)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *dsize = total;
    data[total] = '\0';

    return n > 0 ? (int) total : n;
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
    
    for(p = serverInfo; p != NULL; p = p->ai_next)
    {
        sfd = socket(
                serverInfo->ai_family,
                serverInfo->ai_socktype,
                serverInfo->ai_protocol
        );
        if(INVALID_SOCKET == sfd)
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

    if(INVALID_SOCKET == sfd)
    {
        error("Unable to connect to the server!", NULL, exit);
    }
    
    return sfd;
}

int
runclient(SOCKET sfd)
{
    const int bufsize = 256;
    size_t limit = bufsize - 1;
    char buf[bufsize];
    size_t inplen;
    
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Please enter the message: ");
    buf[limit] = '\0';
    fgets(buf, limit, stdin);
    inplen = strlen(buf);
    
    if(SOCKET_ERROR == send(sfd, buf, (int) inplen, 0))
    {
        error("send() failed", &sfd, exit);
    }
    shutdown(sfd, SD_SEND);
    
    if(SOCKET_ERROR == readn(sfd, buf, &limit))
    {
        fprintf(stderr, "Was read %d bytes, but recv() failed", limit);
        error("", &sfd, exit);
    }
    shutdown(sfd, SD_RECEIVE);
    closesocket(sfd);
    
    printf("%s\n", buf);
    
    return 0;
}

int
main(int argc, char** argv)
{
    if(3 > argc)
    {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    WSADATA wsaData;
    if(0 != WSAStartup(MAKEWORD(2,2), &wsaData)) {
        error("WSAStartup() failed:\n", NULL, exit);
    }
    
    SOCKET sfd = prepareclient(argv[1], argv[2]);
    int rv = runclient(sfd);
    
    WSACleanup();
    
    return rv;
}