#ifndef TERMINAL_H
#define TERMINAL_H

#include <windows.h>

int
terminal_run(void (*stop_server)(void));

void
terminal_stop();

HANDLE
terminal_get_input_event();

int
terminal_handle_action();

#endif