#ifndef _LOG_H_
#define _LOG_H_

#include <time.h>
#include <stdarg.h>
#include "params.h"

FILE *log_open(const char *path);
void Log(const char *format, ...);

#endif
