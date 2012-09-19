/*******************************************************************************
* lisod.c                                                                      *
*                                                                              *
* Description: This file contains the C source code for the Liso server. The   *
*              implementation is originally based on an echo server written by *
*              Athula Balachandran <abalacha@cs.cmu.edu> and Wolf Richter      *
*              <wolf@cs.cmu.edu> from the Course 15441/641 at Carnegie Mellon  *
*              University.                                                     *
*                                                                              *
*              The server currently support following features:                *
*              1. HTTP1.1: support HEAD, GET and POST                          *
*              2. support connections from multiple clients                    *
*              3. log debug, info and error in the log file                    *
*                                                                              *
*                                                                              *
* Authors: Wenjun Zhang <wenjunzh@andrew.cmu.edu>,                             *
*                                                                              *
* Usage:   ./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder>*
*          <CGI folder> <private key> <certificate file>                       *
*                                                                              *
*******************************************************************************/

#include "lisod.h"

struct lisod_state STATE;

int main(int argc, char* argv[])
{
    int sock, client_sock;
    socklen_t client_size;
    struct sockaddr_in addr, client_addr;
    static pool pool;

    init(argc, argv);
    Log("Start Liso server");
    fprintf(stdout, "----- Echo Server -----\n");

    /* all networked programs must create a socket
     * PF_INET - IPv4 Internet protocols
     * SOCK_STREAM - sequenced, reliable, two-way, connection-based byte stream
     * 0 (protocol) - use default protocol
     */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    STATE.sock = sock;
    Log("socket success!");

    addr.sin_family = AF_INET;
    addr.sin_port = htons(STATE.port);
    addr.sin_addr.s_addr = INADDR_ANY;
    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }
    Log("bind success!");

    if (listen(sock, MAX_CONN))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }
    Log("listen success!");

    init_pool(sock, &pool);

    // the main loop to wait for connections and serve request
    while (1)
    {
       pool.ready_set = pool.read_set;
       pool.nready = select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

       if (pool.nready < 0)
       {
           clean();
           fprintf(stderr, "Error select connections.\n");
           Log("Error: select error");
           return EXIT_FAILURE;
       }

       // if there is new connection, accept and add new client to pool
       if (FD_ISSET(sock, &pool.ready_set))
       {
           client_size = sizeof(client_addr);
           client_sock = accept(sock, (struct sockaddr *) &client_addr,
                                &client_size);

           if (client_sock == -1)
           {
               clean();
               fprintf(stderr, "Error accepting connection. \n");
               return EXIT_FAILURE;
           }
           Log("accept client");

           add_client(client_sock, &pool);
       }

       // process each ready connected descriptor
       check_clients(&pool);
    }

    clean();
    return EXIT_SUCCESS;
}

/******************************************************************************
* subroutine: init                                                            *
* purpose:    parse arguments and perform all other initialization            *
* parameters: argc - the number of arguments passed to main                   *
*             argv - the array of arguments passed to main                    *
* return:     none                                                            *
******************************************************************************/
void init(int argc, char* argv[])
{
    if (argc != 9)
        usage_exit();

    // parse arguments
    STATE.port = (int)strtol(argv[1], (char**)NULL, 10);
    STATE.s_port = (int)strtol(argv[2], (char**)NULL, 10);
    strcpy(STATE.log_path, argv[3]);
    strcpy(STATE.lck_path, argv[4]);
    strcpy(STATE.www_path, argv[5]);
    strcpy(STATE.cgi_path, argv[6]);
    strcpy(STATE.key_path, argv[7]);
    strcpy(STATE.ctf_path, argv[8]);

    STATE.log = log_open(STATE.log_path);
}

/******************************************************************************
* subroutine: usage_exit                                                      *
* purpose:    print usage description whenever wrong arguments are passed in  *
* parameters: none                                                            *
* return:     none                                                            *
******************************************************************************/
void usage_exit()
{
    fprintf(stdout,
            "Usage: ./lisod <HTTP port> <HTTPS port> <log file> <lock file> \n"
            "       <www folder> <CGI folder or script name> <private key file> \n"
            "       <certificate file> \n"
            "Command line descriptions: \n"
            "    HTTP port - the port for HTTP server to listen on \n"
            "    HTTPS port - the port for HTTPS server to listen on \n"
            "    log file   - file to send log messages to \n"
            "    lock file  - file to lock on when becoming a daemon process \n"
            "    www folder - folder containing a tree to serve as the root of a website \n"
            "    CGI folder - folder containign CGI programs \n"
            "    private key file - private key file path \n"
            "    certificate file - certificate file path \n"
            );
    exit(EXIT_FAILURE);
}

/******************************************************************************
* subroutine: init_pool                                                       *
* purpose:    setup the initial value for pool attributes                     *
* parameters: sock - the descriptor server using to listen client connections *
*             p    - pointer to pool instance                                 *
* return:     none                                                            *
******************************************************************************/
void init_pool (int sock, pool *p)
{
    // Initialize descriptors
    int i;
    p->maxi = -1;
    for (i=0; i< FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    // Initially, sock is the only member of select read_set
    p->maxfd = sock;
    FD_ZERO(&p->read_set);
    FD_SET(sock, &p->read_set);
}

/******************************************************************************
* subroutine: add_client                                                      *
* purpose:    add a new client to the pool and update pool attributes         *
* parameters: client_fd - the descriptor of new client                        *
*             p    - pointer to pool instance                                 *
* return:     0 on success, -1 on failure                                     *
******************************************************************************/
int add_client(int client_fd, pool *p)
{
    int i;
    p->nready--;
    for (i=0; i<FD_SETSIZE; i++)
    {
        if (p->clientfd[i] < 0)
        {
            // add client descriptor to the pool
            p->clientfd[i] = client_fd;

            // add the descriptor to the descriptor set
            FD_SET(client_fd, &p->read_set);

            // add read buf
             rio_readinitb(&p->clientrio[i], client_fd);

            // update max descriptor and pool highwater mark
            if (client_fd > p->maxfd)
                p->maxfd = client_fd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }
    if (i == FD_SETSIZE)
    {
        Log ("add_client error: too many clients");
        return -1;
    }
    return 0;
}

void remove_client(int client_fd, int id, pool *p)
{
    FD_CLR(client_fd, &p->read_set);
    p->clientfd[id] = -1;
    close(client_fd);
}

/******************************************************************************
* subroutine: check_clients                                                   *
* purpose:    process the ready set from the set of descriptors               *
* parameters: p - pointer to the pool instance                                *
* return:     0 on success, -1 on failure                                     *
******************************************************************************/
void check_clients(pool *p)
{
    int i, connfd;

    for (i=0; (i<= p->maxi) && (p->nready > 0); i++)
    {
        connfd = p->clientfd[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set)))
        {
            p->nready--;
           // echo(connfd, i, p);
           process_request(i, p);
        }
    }
}

/******************************************************************************
* subroutine: process_request                                                 *
* purpose:    handle a single request and return responses                    *
* parameters: client_fd - client descriptor                                   *
* return:     noen                                                            *
******************************************************************************/
void process_request(int id, pool *p)
{
    char *ptr;
    char method[MIN_LINE], version[MIN_LINE]; 
    char buf[MAX_LINE], uri[MAX_LINE], filename[MAX_LINE], cgiargs[MAX_LINE];

    Log("Start processing request.");
    fprintf(stdout, "Start processing request, id=%d fd=%d\n", id, p->clientfd[id]);
    // read and parse request line and headers

    // read request line
    memset(buf, 0, MAX_LINE);
    if (rio_readlineb(&p->clientrio[id], buf, MAX_LINE) < 0)
    {
        Log("rio_readlineb error in process_request");
        return;
    }

    sscanf(buf, "%s %s %s", method, uri, version);
    fprintf(stdout, "method:%s, uri:%s, version:%s \n", method, uri, version);

    // read request headers
    rio_readlineb(&p->clientrio[id], buf, MAX_LINE);
    while(strcmp(buf, "\r\n"))
        rio_readlineb(&p->clientrio[id], buf, MAX_LINE);

    // initialize filename path
    memset(filename, 9, MAX_LINE);
    strcpy(filename, STATE.www_path);
    if (filename[strlen(filename)-1] != '/')
        strcat(filename, "/");

    // parse uri
    if (!strstr(uri, "cgi-bin"))  // static content
    {
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/')
        {
            strcat(filename, "index.html");
        }
    }
    else
    {                             // dynamic content
        ptr = index(uri, '?');
        if (ptr)
        {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0'; ///TODO what is this for?
        }
        else
        {        
            strcpy(cgiargs, "");
        }
    }
    //
    fprintf(stdout, "filename=%s \n", filename); 
    //
    // if methods not implemented, response error
    if (!strcasecmp(method, "HEAD")) 
    {
        serve_head(p->clientfd[id], filename); 
    }
    else if (!strcasecmp(method, "GET"))
    {
        serve_head(p->clientfd[id], filename); 
        serve_body(p->clientfd[id], filename);
    }
    else if (!strcasecmp(method, "POST"))
    {
        serve_head(p->clientfd[id], filename); 
        serve_body(p->clientfd[id], filename);
    }
    else
    {
        serve_error(p->clientfd[id], "501", "Method Unimplemented",
                     "RFC 2068 Hypertext Transfer Protocol - HTTP/1.1 10.5.2");  
    }  
 
    Log("End of processing request.");
}

void serve_head(int client_fd, char *filename)
{
    struct stat sbuf;
    struct tm tm;
    char buf[BUF_SIZE], filetype[MIN_LINE], tbuf[MIN_LINE]; 
    // check file existence
    if (stat(filename, &sbuf) < 0)
    {
        serve_error(client_fd, "404", "Not Found",
                    "Server couldn't find this file");
        return;
    }

    // get time string
    tm = *gmtime(&sbuf.st_mtime);
    strftime(tbuf, MIN_LINE, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // send response headers to client
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Liso/1.0\r\n", buf);
    sprintf(buf, "%sContent-Length: %ld\r\n", buf, sbuf.st_size);
    sprintf(buf, "%sContent-Type: %s\r\n", buf, filetype);
    sprintf(buf, "%sLast-Modified: %s\r\n\r\n", buf, tbuf);
    rio_writen(client_fd, buf, strlen(buf));
}

void serve_body(int client_fd, char *filename)
{
    int fd, filesize;
    char *ptr;
    struct stat sbuf;

    fd = open(filename, O_RDONLY, 0);
    if (fd < 0)
    {
        Log("Error: Cann't open file");
        return;
    }
    if (stat(filename, &sbuf) < 0)
    {
        Log("Error: Cann't stat file");
        return;
    }
    filesize = sbuf.st_size;
    ptr = mmap(0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    rio_writen(client_fd, ptr, filesize); 
    munmap(ptr, filesize);
}


void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

/******************************************************************************
* subroutine: serve_error                                                     *
* purpose:    return error message to client                                  *
* parameters: client_fd: client descriptor                                    *
*             errnum: error number                                            *
*             shortmsg: short error message                                   *
*             longmsg:  long error message                                    *
* return:     none                                                            *
******************************************************************************/
void serve_error(int client_fd, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[BUF_SIZE], body[MAX_LINE];
    memset(buf, 0, BUF_SIZE); memset(body, 0, MAX_LINE);

    // build HTTP response body
    sprintf(body, "<html><title>Lisod Error</title>");
    sprintf(body, "%s<body>\r\n", body);
    sprintf(body, "%sError %s -- %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<br><p>%s</p></body></html>\r\n", body, longmsg);

    // print HTTP response
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    //send(client_fd, buf, strlen(buf), 0);
    rio_writen(client_fd, buf, strlen(buf));
    memset(buf, 0, BUF_SIZE);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client_fd, buf, strlen(buf), 0);
    memset(buf, 0, BUF_SIZE);
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    send(client_fd, buf, strlen(buf), 0);
    send(client_fd, body, strlen(body), 0);
}


/******************************************************************************
* subroutine: close_socket                                                    *
* purpose:    close the given socket                                          *
* parameters: sock - the socket descriptor                                    *
* return:     0 on success, -1 on failure                                     *
******************************************************************************/
int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return -1;
    }
    return 0;
}

/******************************************************************************
* subroutine: clean                                                           *
* purpose:    cleanup allocated resources when server is shutdown             *
* parameters: none                                                            *
* return:     none                                                            *
******************************************************************************/
void clean()
{
    fclose(STATE.log);
    close_socket(STATE.sock);
}

/******************************************************************************
 *                            wrappers from csapp                             *
 *****************************************************************************/

/* 
 * rio_read - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) {  /* refill if buf is empty */
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0) {
            if (errno != EINTR) /* interrupted by sig handler return */
                return -1;
        }
        else if (rp->rio_cnt == 0)  /* EOF */
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf; /* reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;
    if (rp->rio_cnt < n)
        cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

/*
 * rio_readinitb - Associate a descriptor with a read buffer and reset buffer
 */
void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}

/* 
 * rio_readlineb - robustly read a text line (buffered)
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) {
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n')
                break;
        } else if (rc == 0) {
            if (n == 1)
                return 0; /* EOF, no data read */
            else
                break;    /* EOF, some data was read */
        } else
            return -1;    /* error */
    }
    *bufp = 0;
    return n;
}
/*
 * rio_writen - robustly write n bytes (unbuffered)
 */
ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* interrupted by sig handler return */
		nwritten = 0;    /* and call write() again */
	    else
		return -1;       /* errorno set by write() */
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}

/******************************************************************************
* subroutine: echo                                                            *
* purpose:    return any message client send to server                        *
* parameters: connfd - connection descriptor                                  *
* return:     0 on success, -1 on failure                                     *
******************************************************************************/
int echo(int connfd, int id, pool *p)
{
    ssize_t readret = 0;
    char buf[BUF_SIZE];

    if ((readret = recv(connfd, buf, BUF_SIZE, MSG_DONTWAIT)) > 0)
    {
        if (send(connfd, buf, readret, 0) != readret)
        {
            fprintf(stderr, "Error sending to client.\n");
            Log("Error sending to client.");
            return -1;
        }
        buf[readret-1] = '\0';
        Log(buf);

        memset(buf, 0, BUF_SIZE);
    }

    if (readret == 0)
        remove_client(connfd, id, p);
    else if (readret == -1)
    {
        Log("Error: recv error");
        return -1;
    }
    return 0;
}
