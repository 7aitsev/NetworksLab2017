#ifndef TERMPROTO_H
#define TERMPROTO_H

#define TERMPROTO_PATH_SIZE 256
#define TERMPROTO_BUF_SIZE 1024

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
    REQ_TIMEOUT = 10,
    INTERNAL_ERROR = 12
};

struct term_req {
    unsigned short int seq;
    enum TERM_METHOD method;
    char path[TERMPROTO_PATH_SIZE];
    enum TERM_STATUS status;

    const char* msg; // detailed status information;
};

char*
term_get_method_str(int method);

char*
term_get_status_str(int status);

int
term_is_valid_method(const char* method);

int
term_parse_req(struct term_req* req, const char* buf);

int
term_put_header(char* buf, int bufsize, unsigned int seq,
    enum TERM_STATUS status);

int
term_mk_req_header(struct term_req* req, char* buf, int bufsize);

int
term_parse_resp_status(struct term_req* req, char* buf);

#endif
