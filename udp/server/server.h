#ifndef SERVER_H
#define SERVER_H

int
server_init(const char* host, const char* port);

void
server_run();

void
server_stop();

void
server_destroy();

#endif
