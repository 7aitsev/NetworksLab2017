#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <windows.h>

#define LOGGER_BUFFER_SIZE 1024
#define LOGGER_QUEUE_THRESHOLD 4
#define LOGGER_SLEEP_TIME 50

struct logdata
{
    int ld_isrunning;
    int ld_isready;
    char* ld_buffer;
    CRITICAL_SECTION ld_mx;
    CONDITION_VARIABLE ld_cv;
};

struct logger
{
    int l_buflen;
    int l_msgcnt;
    char* l_buffer;
    HANDLE l_tid;
    CRITICAL_SECTION l_sp;
    struct logdata* l_ld;
};

static struct logger g_logger;

static void
swapbufs(char** a, char** b)
{
    char* tmp = *a;
    *a = *b;
    *b = tmp;
}

static void
waitfor(int* condition)
{
    while(1 == __sync_and_and_fetch(condition, 1))
    {
        usleep(LOGGER_SLEEP_TIME);
    }
}

void
logger_log(const char* format, ...)
{
    static const int limit = LOGGER_BUFFER_SIZE;

    EnterCriticalSection(&g_logger.l_sp);
    
    struct logdata* ld = g_logger.l_ld;

    // if logger_loop has not handled messages yet
    waitfor(&ld->ld_isready);

    va_list args;
    int bytes;
    int* buflen = &g_logger.l_buflen;
    int bytesleft = limit - *buflen;
    int* msgcnt = &g_logger.l_msgcnt;
    char** buf = &g_logger.l_buffer;
    int wastruncated = 0;

    va_start(args, format);
    bytes = vsnprintf(*buf + *buflen, bytesleft, format, args);
    va_end(args);
    *buflen += bytes;

    if(bytes >= bytesleft)
    {
        sprintf(*buf + limit - 7, "<...>\n");
        wastruncated = 1;
    }

    if(LOGGER_QUEUE_THRESHOLD == ++(*msgcnt) || 1 == wastruncated)
    {
        *msgcnt = 0;
        *buflen = 0;

        EnterCriticalSection(&ld->ld_mx);
        swapbufs(buf, &ld->ld_buffer);
        ld->ld_isready = 1;
        WakeConditionVariable(&ld->ld_cv);
        LeaveCriticalSection(&ld->ld_mx);       
    }

    LeaveCriticalSection(&g_logger.l_sp);
}

DWORD WINAPI
logger_loop(LPVOID p_logdata)
{
    struct logdata* ld = (struct logdata*) p_logdata;

    __sync_add_and_fetch(&ld->ld_isrunning, 1);
    while(__sync_fetch_and_or(&ld->ld_isrunning, 0))
    {
        EnterCriticalSection(&ld->ld_mx);
        while(0 == __sync_fetch_and_or(&ld->ld_isready, 0))
        {
            SleepConditionVariableCS(&ld->ld_cv, &ld->ld_mx, INFINITE);
        }
        if(__sync_fetch_and_or(&ld->ld_isrunning, 0))
            fprintf(stderr, ld->ld_buffer);
        __sync_and_and_fetch(&ld->ld_isready, 0);
        LeaveCriticalSection(&ld->ld_mx);
    }

    return 0;
}

static void
logger_data_init(struct logdata* logbundle)
{
    logbundle->ld_isready = 0;
    logbundle->ld_isrunning = 0;
    logbundle->ld_buffer = ((char*) (logbundle)) + sizeof(struct logdata);

    InitializeCriticalSection(&logbundle->ld_mx);
    InitializeConditionVariable(&logbundle->ld_cv);
}

void
logger_init()
{
    struct logger* p = &g_logger;
    if(NULL == p->l_tid)
    {
        p->l_ld = malloc(sizeof(struct logdata) + 2 * LOGGER_BUFFER_SIZE);
        memset(p->l_ld, 0, sizeof(struct logdata) + 2 * LOGGER_BUFFER_SIZE);
        logger_data_init(p->l_ld);

        p->l_buflen = 0;
        p->l_msgcnt = 0;
        p->l_buffer = p->l_ld->ld_buffer + LOGGER_BUFFER_SIZE;
        InitializeCriticalSectionAndSpinCount(&p->l_sp, LOGGER_SLEEP_TIME);

        p->l_tid = CreateThread(NULL, 0, logger_loop, p->l_ld, 0, NULL);
    }
}

static void
logger_data_destroy(struct logdata* logbundle)
{
    DeleteCriticalSection(&logbundle->ld_mx);
    // no delete function is needed for CONDITION_VARIABLE
}

void
logger_flush()
{
    EnterCriticalSection(&g_logger.l_sp);
    struct logdata* ld = g_logger.l_ld;
    int* buflen = &g_logger.l_buflen;
    
    waitfor(&ld->ld_isready);

    if(0 < *buflen)
    {
        int* msgcnt = &g_logger.l_msgcnt;
        char** buf = &g_logger.l_buffer;

        *buflen = 0;
        *msgcnt = 0;

        EnterCriticalSection(&ld->ld_mx);  
        swapbufs(buf, &ld->ld_buffer);
        ld->ld_isready = 1;
        WakeConditionVariable(&ld->ld_cv);
        LeaveCriticalSection(&ld->ld_mx);  

        waitfor(&ld->ld_isready);
    }
    LeaveCriticalSection(&g_logger.l_sp);
}

void
logger_destroy()
{
    struct logger* p = &g_logger;
    
    if(NULL != p->l_tid)
    {
        logger_flush();

        EnterCriticalSection(&p->l_ld->ld_mx);
        __sync_add_and_fetch(&p->l_ld->ld_isready, 1);
        __sync_sub_and_fetch(&p->l_ld->ld_isrunning, 1);
        WakeConditionVariable(&p->l_ld->ld_cv);
        LeaveCriticalSection(&p->l_ld->ld_mx);
        WaitForSingleObject(p->l_tid, INFINITE);

        logger_data_destroy(p->l_ld);
        DeleteCriticalSection(&p->l_sp);
        free(p->l_ld);
        p->l_tid = NULL;
    }
}
