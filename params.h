#ifndef _PARAMS_H_
#define _PARAMS_H_

#include <stdio.h>
#include <stdlib.h>

struct lisod_state
{
    FILE* logfile;
    int port;
    int sock;
};

extern struct lisod_state STATE;

#endif

