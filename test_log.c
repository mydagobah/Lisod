#include <stdio.h>
#include <stdlib.h>

#include "log.h"

// usage: gcc log.c test_log.c -o testlog
void main()
{
    FILE *logfile;
    logfile = log_open();

    Log(logfile, "messge 1");
    Log(logfile, "message 2");
    fclose(logfile);
   
    exit(0);
}
