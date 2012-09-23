/*
 * log.c
 *
 * Description: This file defines routines to record logs for Liso server
 *
 */
#include "log.h"

FILE *log_open(const char *path)
{
    FILE *logfile;

    logfile = fopen(path, "w");
    if ( logfile == NULL )
    {
        fprintf(stdout, "Error opening logfile. \n");
        exit(EXIT_FAILURE);
    }

    // set logfile to line buffering
    setvbuf(logfile, NULL, _IOLBF, 0);

    return logfile;
}

void Log(const char *format, ...)
{
    time_t ltime;
    struct tm *Tm;

    ltime = time(NULL);
    Tm = localtime(&ltime);

    fprintf(STATE.log, "[%04d%02d%02d %02d:%02d:%02d] ",
                 Tm->tm_year+1900,
                 Tm->tm_mon+1,
                 Tm->tm_mday,
                 Tm->tm_hour,
                 Tm->tm_min,
                 Tm->tm_sec
           );

    va_list ap;
    va_start(ap, format);
    vfprintf(STATE.log, format, ap);
}
