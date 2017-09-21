#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define LOGGER_BUFFER_SIZE 1024
#define LOGGER_QUEUE_THRESHOLD 4
#define LOGGER_SLEEP_TIME 50

struct logdata
{
    int ld_isready;
    char* ld_buffer;
    pthread_mutex_t ld_mx;
    pthread_cond_t ld_cv;
};

struct logger
{
    int l_buflen;
    int l_msgcnt;
    char* l_buffer;
    pthread_t l_tid;
    pthread_spinlock_t l_sp;
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
    while(1 == *condition)
    {
        usleep(LOGGER_SLEEP_TIME);
    }
}

void
logger_log(const char* format, ...)
{
    static const int limit = LOGGER_BUFFER_SIZE;

    pthread_spin_lock(&g_logger.l_sp);
    
    struct logdata* ld = g_logger.l_ld;

    // logger_loop does not handle messages
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

        pthread_mutex_lock(&ld->ld_mx);
        swapbufs(buf, &ld->ld_buffer);
        ld->ld_isready = 1;
        pthread_cond_signal(&ld->ld_cv);
        pthread_mutex_unlock(&ld->ld_mx);       
    }

    va_end(args);
    pthread_spin_unlock(&g_logger.l_sp);
}

static void*
logger_loop(void* p_logdata)
{
    struct logdata* ld = (struct logdata*) p_logdata;

    while(1)
    {
        pthread_mutex_lock(&ld->ld_mx);
        while(0 == ld->ld_isready)
        {
            pthread_cond_wait(&ld->ld_cv, &ld->ld_mx);
            pthread_testcancel();
        }
        fprintf(stdout, ld->ld_buffer);
        ld->ld_isready = 0;
        pthread_mutex_unlock(&ld->ld_mx);
    }

    pthread_exit(NULL);
}

static void
logger_data_init(struct logdata* logbundle)
{
    logbundle->ld_isready = 0;
    logbundle->ld_buffer = (char*) malloc(2 * LOGGER_BUFFER_SIZE); 

    pthread_mutex_init(&logbundle->ld_mx, NULL);
    pthread_cond_init(&logbundle->ld_cv, NULL);
}

void
logger_init()
{
    struct logger* p = &g_logger;
    if(0 == p->l_tid)
    {
        p->l_ld = (struct logdata*) malloc(sizeof(struct logdata));
        logger_data_init(p->l_ld);

        p->l_buflen = 0;
        p->l_msgcnt = 0;
        p->l_buffer = p->l_ld->ld_buffer + LOGGER_BUFFER_SIZE;
        pthread_spin_init(&p->l_sp, PTHREAD_PROCESS_PRIVATE);

        pthread_create(&p->l_tid, NULL, logger_loop, p->l_ld);
    }
}

static void
logger_data_destroy(struct logdata* logbundle)
{
    pthread_mutex_destroy(&logbundle->ld_mx);
    pthread_cond_destroy(&logbundle->ld_cv);
}

void
logger_flush()
{
    pthread_spin_lock(&g_logger.l_sp);
    struct logdata* ld = g_logger.l_ld;
    int* buflen = &g_logger.l_buflen;
    
    waitfor(&ld->ld_isready);

    if(0 < *buflen)
    {
        int* msgcnt = &g_logger.l_msgcnt;
        char** buf = &g_logger.l_buffer;

        *buflen = 0;
        *msgcnt = 0;

        pthread_mutex_lock(&ld->ld_mx);  
        swapbufs(buf, &ld->ld_buffer);
        ld->ld_isready = 1;
        pthread_cond_signal(&ld->ld_cv);
        pthread_mutex_unlock(&ld->ld_mx);  

        waitfor(&ld->ld_isready);
    }
    pthread_spin_unlock(&g_logger.l_sp);
}

void
logger_destroy()
{
    struct logger* p = &g_logger;
    char* bufone = p->l_ld->ld_buffer;
    char* buftwo = p->l_buffer;
    
    logger_flush();

    pthread_mutex_lock(&p->l_ld->ld_mx);
    pthread_cancel(p->l_tid);
    pthread_cond_signal(&p->l_ld->ld_cv);
    pthread_mutex_unlock(&p->l_ld->ld_mx);
    pthread_join(p->l_tid, NULL);

    free(buftwo - bufone > 0 ? bufone : buftwo);
    logger_data_destroy(p->l_ld);
    pthread_spin_destroy(&p->l_sp);
}
