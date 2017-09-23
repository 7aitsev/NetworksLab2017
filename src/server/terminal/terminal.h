#ifndef TERMINAL_H
#define TERMINAL_H

#include <pthread.h>

struct termdata
{
    pthread_t td_tid;
    void (*td_stopserver)(void);
};

void
terminal_setstopservercb(void (*stop_server)(void));

void
terminal_run();

void
terminal_join();

#endif
