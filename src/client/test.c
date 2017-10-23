#include <stdio.h>

#include <winsock2.h>
#include <ws2tcpip.h>

void
error(const char* errMsg)
{
    char *decErrCode = NULL;
    if(0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            0, (DWORD) WSAGetLastError(), 0, (LPSTR) &decErrCode, 0, 0))
    {
        *decErrCode = '\0';
    }
    fprintf(stderr, ":\n%s %s\n", errMsg, decErrCode);
}

int
main(void)
{
    WSADATA wsaData;
    if(0 != WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        perror("WSAStartup() failed");
        return 1;
    }
    
    SOCKET s;
    
    if(-1 == send(s, "test", 4, 0))
    {
        perror("send failed");
        error("send failed");
        return 2;
    }
    
    WSACleanup();
    return 0;
}