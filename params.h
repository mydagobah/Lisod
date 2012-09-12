#ifndef _PARAMS_H_
#define _PARAMS_H_

#include <stdio.h>
#include <stdlib.h>

#define MAX_NAME_LEN 255
#define MAX_PATH_LEN 4096
struct lisod_state
{
    int  port;
    int  s_port;
    char log_path[MAX_PATH_LEN];
    char lck_path[MAX_PATH_LEN];
    char www_path[MAX_PATH_LEN];
    char cgi_path[MAX_PATH_LEN];
    char key_path[MAX_PATH_LEN];
    char ctf_path[MAX_PATH_LEN];
    int  sock;
    FILE* log;
};

extern struct lisod_state STATE;

#endif

