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
#include "log.h"

typedef struct
{
    int rio_fd;                 // descriptor for this internal buf 
    int rio_cnt;                // unread bytes in internal buf 
    char *rio_bufptr;           // next unread byte in internal buf 
    char rio_buf[MAX_LINE];     // internal buffer 
} rio_t;

/* represent a pool of connected client descriptors */
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

/* declaration of subroutines */
void init(int argc, char* argv[]);
void init_pool(int sock, pool *p);
int  add_client(int client_fd, pool *p);
void remove_client(int client_fd, int index, pool *p);
void check_clients(pool *p);
void process_request(int id, pool *p); 
void serve_error(int client_fd, char *errnum, char *shortmsg, char *longmsg);
void serve_head(int client_fd, char *filename);
void get_filetype(char *filename, char *filetype);
int  echo(int connfd, int id, pool *p);
int  close_socket(int sock);
void clean();
void usage_exit();

// wrappers from csapp
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fd);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);

#endif
