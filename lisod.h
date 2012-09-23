#ifndef _LISOD_H_
#define _LISOD_H_

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include "log.h"

/* this data structure wraps some attributes used for sending data with client */
typedef struct
{
    int rio_fd;                 // descriptor for this internal buf 
    int rio_cnt;                // unread bytes in internal buf 
    char *rio_bufptr;           // next unread byte in internal buf 
    char rio_buf[MAX_LINE];     // internal buffer 
} rio_t;

/* this data struture wraps some attributes used to manage a pool of connected 
 * clients. (originally from CSAPP)*/
typedef struct
{
    int maxfd;                   // Largest descriptor in read_set
    int nready;                  // Number of ready descriptors from select
    int maxi;                    // Highwater index into client array
    int clientfd[FD_SETSIZE];    // Set of active client descriptors
    fd_set read_set;             // Set of all active descriptors
    fd_set ready_set;            // Subset of descriptors ready for reading
    rio_t clientrio[FD_SETSIZE]; // Set of active read buffers
} pool;

/* this datastructure wraps some attributes used for processing HTTP requests */
typedef struct
{
    int  is_secure;
    int  is_static;
    int  content_len;
    char method[MIN_LINE];
    char version[MIN_LINE];
    char uri[MAX_LINE];
    char filename[MAX_LINE];
    char cgiargs[MAX_LINE];
} HTTPContext;

/* declaration of subroutines */
void clean();
void usage_exit();
void lisod_shutdown();
void signal_handler(int sig);
void daemonize();
int  close_socket(int sock);

void init_pool(pool *p);
int  add_client(int client_fd, pool *p);
void remove_client(int index, pool *p);
void check_clients(pool *p);

void process_request(int id, pool *p, int *is_closed); 
int  parse_requestline(int id, pool *p, HTTPContext *context, int *is_closed);
void parse_uri(HTTPContext *context);
int  parse_requestheaders(int id, pool *p, HTTPContext *context, int *is_closed);
int parse_requestbody(int id, pool *p, HTTPContext *context, int *is_closed);
void serve_head(int client_fd, HTTPContext *context, int *is_closed);
void serve_get(int client_fd, HTTPContext *context,  int *is_closed);
void serve_post(int client_fd, HTTPContext *context,  int *is_closed);
int  serve_body(int client_fd, HTTPContext *context, int *is_closed);
void serve_error(int client_fd, char *errnum, char *shortmsg, char *longmsg, int is_closed);

int  validate_file(int client_d, HTTPContext *context, int *is_closed);
void get_filetype(char *filename, char *filetype);

// wrappers from csapp
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fd);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

#endif
