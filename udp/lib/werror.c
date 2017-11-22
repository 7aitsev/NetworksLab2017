#include "werror.h"

#include <stdio.h>
#include <windows.h>

/** not thread-safe */
char*
wstrerror()
{
    static char buf[1024];
    if(0 == FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(), 0, buf, 1024, NULL))
    {
        sprintf(buf, "FormatMessage() failed: err=0x%lx\n", GetLastError());
    }
    return buf;
}