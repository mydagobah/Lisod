#ifndef _PARAMS_H_
#define _PARAMS_H_

#include <stdio.h>
#include <stdlib.h>

#define MIN_LINE 64
#define MAX_NAME 256
#define MAX_CONN 1024
#define BUF_SIZE 4096
#define MAX_PATH 4096
#define MAX_LINE 8192

struct lisod_state
{
    FILE* log;
    int  is_full;
    int  port;
    int  s_port;
    int  sock;
    int  s_sock;
    char log_path[MAX_PATH];
    char lck_path[MAX_PATH];
    char www_path[MAX_PATH];
    char cgi_path[MAX_PATH];
    char key_path[MAX_PATH];
    char ctf_path[MAX_PATH];
};

extern struct lisod_state STATE;

#endif

