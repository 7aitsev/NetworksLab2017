#ifndef TERMPROTO_H
#define TERMPROTO_H

#include <stddef.h>

#define TERMPROTO_PATH_SIZE 256
#define TERMPROTO_BUF_SIZE 1024

enum TERM_METHOD {
    AUTH, LS, CD, KILL, WHO, LOGOUT
};

enum TERM_STATUS {
    OK = 0,
    BAD_REQUEST = 2,
    FORBIDDEN = 4,
    NOT_FOUND = 6,
    NOT_DIR = 8,
    REQ_TIMEOUT = 10,
    INTERNAL_ERROR = 12
};

struct term_req {
    enum TERM_METHOD method;
    char path[TERMPROTO_PATH_SIZE];
    enum TERM_STATUS status;

    const char* msg; // detailed status information;
};

int
term_parse_req(struct term_req* term_req, const char* buf);

size_t
term_put_header(char* buf, size_t bufsize, enum TERM_STATUS status,
        size_t size);

#endif
