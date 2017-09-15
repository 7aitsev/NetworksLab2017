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

    return SOCKET_ERROR == n ? EXIT_FAILURE : EXIT_SUCCESS;
}

SOCKET
preparewinserver()
{
	SOCKET master = INVALID_SOCKET;
	
	int yes = 1;
    struct addrinfo hints;
    struct addrinfo* serverInfo;
    struct addrinfo* p;
	
	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	
	if(0 != getaddrinfo(NULL, "5001", &hints, &serverInfo))
	{
		error("getaddrinfo() failed", NULL, exit);
	}
	
    for(p = serverInfo; NULL != p; p = p->ai_next)
	{
        master = socket(
                serverInfo->ai_family,
                serverInfo->ai_socktype,
                serverInfo->ai_protocol
        );
        if(INVALID_SOCKET == master)
		{
            continue;
        }
		        
        if(0 != setsockopt(master, SOL_SOCKET, SO_REUSEADDR,
				(char*) &yes, sizeof(yes)))
		{
            error("#setsockopt failed:\n", NULL, exit);
        }
        
        if(0 != bind(master, p->ai_addr, p->ai_addrlen))
		{
            error("#bind failed:\n", &master, NULL);
            continue;
        }
        break;
    }
    freeaddrinfo(serverInfo);
	
    if(NULL == p)
	{
        error("bind() failed", NULL, exit);
    }

    if(0 != listen(master, 5))
	{
        error("#listen failed:\n", &master, exit);
    }
	
	return master;
}

int
runwinserver(SOCKET master)
{
	SOCKET slave;
	struct sockaddr_storage slaveaddr;
	int slaveaddrlen = sizeof(slaveaddr);
	const int bufsize = 256;
	size_t toread = bufsize - 1;
	char buf[bufsize];
	
	slave = accept(master, (struct sockaddr*) &slaveaddr, &slaveaddrlen);
	if(INVALID_SOCKET == slave)
	{
		error("accept() failed", &master, exit);
	}
	shutdown(master, SD_BOTH);
	closesocket(master);
	
	if(0 != readn(slave, buf, &toread))
	{
		fprintf(stderr, "Was read %d bytes, but recv() failed", toread);
		error("", &slave, exit);
	}
	shutdown(slave, SD_RECEIVE);
	
	printf("Here is the message: %s\n", buf);
	
	if(SOCKET_ERROR == send(slave, "I got your message", 18, 0))
	{
		error("send() failed", &slave, exit);
	}
	shutdown(slave, SD_SEND);
	
	closesocket(slave);
	return 0;
}

int
main(void)
{
    WSADATA wsaData;
    if(0 != WSAStartup(MAKEWORD(2,2), &wsaData)) {
		error("WSAStartup() failed:\n", NULL, exit);
    }
	
	SOCKET master = preparewinserver();
	int rv = runwinserver(master);
	
	WSACleanup();
	
	return rv;
}