#ifndef TERMPROTO_H
#define TERMPROTO_H

#define TERMPROTO_PATH_SIZE 256
#define TERMPROTO_BUF_SIZE 1024

enum TERM_METHOD {
    AUTH, LS, KILL, WHO, LOGOUT, HEAD
};

enum TERM_STATUS {
    OK = 0,
    BAD_REQUEST = 2,
    FORBIDDEN = 4,
    NOT_FOUND = 6,
    INTERNAL_ERROR = 8
};

struct TERM_REQ {
    enum TERM_METHOD method;
    char path[TERMPROTO_PATH_SIZE];
    enum TERM_STATUS status;
};

void 
term_mk_resp();

#endif
