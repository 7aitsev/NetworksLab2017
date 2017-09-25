/*
 * This is a cheesy version of a configuration module
 * which is going to provide an API for reading CLI arguments
 * with options for a server and its other components.
 */
#include "config/config.h"
#include "logger/logger.h"
#include "server/server.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SERVER_HOST NULL
#define SERVER_PORT "5001"
#define SERVER_BACKLOG 5

void
print_help()
{
    printf(
"Usage: server tcp [host] [port] [backlog]\n"
"          - start the TCP server\n"
"       server udp [host] [port]\n"
"          - start the UDP server\n\n");
}

void
config_server(int argc, char** argv)
{
    int idx = 0;
    int backlog = 0;
    char* host = NULL;
    char* port = NULL;

    if(argc < 2)
    {
        logger_log("[config] Not enough arguments\n");
        print_help();
        return;
    }

    while(++idx < argc)
    {
        char* option = argv[idx];
        switch(idx)
        {
            case 1:
                if(0 == strcmp(option, "udp"))
                    server_setprotocol(SERVER_UDP);
                else
                    server_setprotocol(SERVER_TCP);
                break;
            case 2:
                host = option;
                break;
            case 3:
                port = option;
                break;
            case 4:
                backlog = atoi(option);
                break;
            default:
                logger_log("[config] Unsupported argument\n");
                print_help();
                return;
        }
    }
    server_sethost((host) ? host : SERVER_HOST);
    server_setport((port) ? port : SERVER_PORT);
    server_setbacklog((backlog) ? backlog : SERVER_BACKLOG);
}
