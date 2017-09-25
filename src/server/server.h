#ifndef SERVER_H
#define SERVER_H

enum server_proto
{
    SERVER_TCP, SERVER_UDP
};

void
server_sethost(char* hostname);

void
server_setport(char* port);

void
server_setbacklog(int backlog);

void
server_setprotocol(int proto);

int
server_prepare();

void
server_run();

void
server_stop();

void
server_join();

#endif
