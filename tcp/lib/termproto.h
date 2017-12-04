#ifndef TERMPROTO_H
#define TERMPROTO_H

#include <stddef.h>

#define TERMPROTO_PATH_SIZE 256
#define TERMPROTO_BUF_SIZE 1024

typedef unsigned short int msgsize_t;

enum TERM_METHOD {
    AUTH, LS, CD, KILL, WHO, LOGOUT
};

enum TERM_STATUS {
    UNDEFINED = -1,
    OK = 0,
    BAD_REQUEST = 2,
    FORBIDDEN = 4,
    NOT_FOUND = 6,
    NOT_DIR = 8,
    INTERNAL_ERROR = 10
};

struct term_req {
    enum TERM_METHOD method;
    char path[TERMPROTO_PATH_SIZE];
    enum TERM_STATUS status;

    const char* msg; // detailed status information;
};

char*
term_get_method(int method);

char*
term_get_status_desc(int status);

int
term_is_valid_method(const char* method);

int
term_parse_req(struct term_req* term_req, const char* buf);

msgsize_t
term_put_header(char* buf, msgsize_t bufsize, enum TERM_STATUS status,
        msgsize_t size);

size_t
term_mk_req_header(struct term_req* req, char* buf, msgsize_t bufsize);

int
term_parse_resp_status(struct term_req* req, char* buf);

msgsize_t
term_parse_resp_body(char* buf);

#endif
