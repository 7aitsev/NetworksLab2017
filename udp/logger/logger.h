#ifndef LOGGER_H
#define LOGGER_H

void
logger_log(const char* format, ...);

void
logger_flush();

void
logger_init();

void
logger_destroy();

#endif