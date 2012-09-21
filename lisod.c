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
*              2. Support connections from multiple clients                    *
*              3. Log debug, info and error in the log file                    *
*              4. Run server as a daemon process                               *
*                                                                              *
* Authors:     Wenjun Zhang <wenjunzh@andrew.cmu.edu>,                         *
*                                                                              *
* Usage:       ./lisod <HTTP port> <HTTPS port> <log file> <lock file>         *
*              <www folder> <CGI folder> <private key> <certificate file>      *
* example:     ./lisod 8080 4443 lisod.log lisod.lock www cgi key cert         *
*                                                                              *
*              To stop the server, first find the pid                          *
*                 ps -ef | grep lisod                                          *
*              then kill the process,                                          *
*                 kill <pid>                                                   *
*******************************************************************************/

#include "lisod.h"

struct lisod_state STATE;
static int KEEPON = 1;

int main(int argc, char* argv[])
{
    int sock, client_fd;
    socklen_t client_size;
    struct sockaddr_in addr, client_addr;
    struct timeval tv;
    static pool pool;
    sigset_t mask;

    if (argc != 9)  usage_exit();

    // parse arguments
    STATE.port = (int)strtol(argv[1], (char**)NULL, 10);
    STATE.s_port = (int)strtol(argv[2], (char**)NULL, 10);
    strcpy(STATE.log_path, argv[3]);
    strcpy(STATE.lck_path, argv[4]);
    strcpy(STATE.www_path, argv[5]);
    strcpy(STATE.cgi_path, argv[6]);
    strcpy(STATE.key_path, argv[7]);
    strcpy(STATE.ctf_path, argv[8]);

    if (STATE.www_path[strlen(STATE.www_path)-1] == '/')
         STATE.www_path[strlen(STATE.www_path)-1] = '\0';

    daemonize();

    STATE.log = log_open(STATE.log_path);
    
    Log("Start Liso server");


    /* all networked programs must create a socket
     * PF_INET - IPv4 Internet protocols
     * SOCK_STREAM - sequenced, reliable, two-way, connection-based byte stream
     * 0 (protocol) - use default protocol
     */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        Log("Error: failed creating socket.\n");
        fclose(STATE.log);
        return EXIT_FAILURE;
    }

    STATE.sock = sock;
    Log("Create socket success!");

    addr.sin_family = AF_INET;
    addr.sin_port = htons(STATE.port);
    addr.sin_addr.s_addr = INADDR_ANY;
    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        Log("Error: failed binding socket.\n");
        clean();
        return EXIT_FAILURE;
    }
    Log("bind success!");

    if (listen(sock, MAX_CONN))
    {
        Log("Error: listening on socket.\n");
        clean();
        return EXIT_FAILURE;
    }
    Log("listen success! >>>>>>>>>>>>>>>>>>>>");

    init_pool(sock, &pool);

    // the main loop to wait for connections and serve requests
    while (KEEPON)
    {
       tv.tv_sec = 1; // timeout = 1 sec
       tv.tv_usec = 0;
       pool.ready_set = pool.read_set;

       sigemptyset(&mask);
       sigaddset(&mask, SIGHUP);
       sigprocmask(SIG_BLOCK, &mask, NULL);
       pool.nready = select(pool.maxfd+1, &pool.ready_set, NULL, NULL, &tv);
       sigprocmask(SIG_UNBLOCK, &mask, NULL);

       if (pool.nready < 0)
       {
           if (errno == EINTR)
           {
               Log("Shut down Server >>>>>>>>>>>>>>>>>>>>");
               break;
           }
          
           Log("Error: select error");
           continue;
       }

       // if there is new connection, accept and add the new client to pool
       if (FD_ISSET(sock, &pool.ready_set))
       {
           client_size = sizeof(client_addr);
           client_fd = accept(sock, (struct sockaddr *) &client_addr,
                              &client_size);

           if (client_fd < 0) ///TODO
           {
               Log("Error: accepting connection. \n");
               continue;
           }

           Log("accept client");

           if (STATE.is_full)
           {
               pool.nready--;
               serve_error(client_fd, "503", "Service Unavailable",
                    "Server is too busy right now. Please try again later.", 1);
               close(client_fd);
           }
           else
              add_client(client_fd, &pool);
       }

       // process each ready connected descriptor
       check_clients(&pool);
    }

    lisod_shutdown();

    return 0; // to make compiler happy
}

void lisod_shutdown()
{
    Log("cleaning up.......");
    clean();
    exit(EXIT_SUCCESS);
}

void daemonize()
{
    int i, lfp, pid;
    char str[256] = {0};
    
    // drop to have init as parent process
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    // obtain a new process group 
    setsid();

    // close all descriptor
    for (i=getdtablesize(); i>=0; i--)  close(i);

    // redirect stdio
    i = open("dev/null", O_RDWR);
    dup(i); //stdout
    dup(i); //stderr

    // set newly created file permissons
    umask(027);
 
    // setup lock file
    lfp = open(STATE.lck_path, O_RDWR|O_CREAT|O_EXCL, 0640);

    if (lfp < 0) exit(EXIT_FAILURE);
    if(lockf(lfp, F_TLOCK, 0) < 0) exit(EXIT_SUCCESS);

    // first instance continues
    sprintf(str, "%d\n", getpid());
    write(lfp, str, strlen(str)); // record pid to lockfile

    signal(SIGCHLD, SIG_IGN); // ignore 

    signal(SIGHUP, signal_handler);  // install hangup signal
    signal(SIGTERM, signal_handler); // kill signal
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
    STATE.is_full = 0;
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

    if (STATE.is_full) return -1;
 
    // only accept FD_SETSIZE - 5 clients to keep server from overloading
    for (i=0; i<(FD_SETSIZE - 5); i++)
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
    
    if (i == (FD_SETSIZE - 5))
    {   
        STATE.is_full = 1;
        Log ("Error: too many clients.");
        return -1;
    }
    return 0;
}

/******************************************************************************
* subroutine: remove_client                                                   *
* purpose:    remove a client from the pool after close a connection          *
* parameters: id - the index number of the client in the pool                 *
*             p - pointer to the pool instance                                *
* return:     none                       `                                    *
******************************************************************************/
void remove_client(int id, pool *p)
{
    FD_CLR(p->clientfd[id], &p->read_set);
    if (close(p->clientfd[id]) < 0) Log("Error: close client fd error");
    p->clientfd[id] = -1;
    STATE.is_full = 0;
}

/******************************************************************************
* subroutine: check_clients                                                   *
* purpose:    process the ready set from the set of descriptors               *
* parameters: p - pointer to the pool instance                                *
* return:     0 on success, -1 on failure                                     *
******************************************************************************/
void check_clients(pool *p)
{
    int i, connfd, is_closed;

    for (i=0; (i <= p->maxi) && (p->nready > 0); i++)
    {
        connfd = p->clientfd[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set)))
        {
            p->nready--;
            is_closed = 0;
            process_request(i, p, &is_closed);
            if (is_closed) remove_client(i, p);
        }
    }
}

/******************************************************************************
* subroutine: process_request                                                 *
* purpose:    handle a single request and return responses                    *
* parameters: id        - the index of the client in the pool                 *
*             p         - a pointer of pool struct                            *
*             is_closed - idicator if the transaction is closed               *
* return:     none                                                            *
******************************************************************************/
void process_request(int id, pool *p, int *is_closed)
{
    HTTPContext *context = (HTTPContext *)calloc(1, sizeof(HTTPContext));

    Log("Start processing request.");

    // parse request line (get method, uri, version)
    if (parse_requestline(id, p, context, is_closed) < 0) goto Done;

    // check HTTP method (support GET, POST, HEAD now)
    if (strcasecmp(context->method, "GET")  && 
        strcasecmp(context->method, "HEAD") && 
        strcasecmp(context->method, "POST"))
    {
        *is_closed = 1;
        serve_error(p->clientfd[id], "501", "Not Implemented",
                   "The method is not valid or not implemented by the server",
                    *is_closed); 
        goto Done;
    }

    // check HTTP version
    if (strcasecmp(context->version, "HTTP/1.1"))
    {
        *is_closed = 1;
        serve_error(p->clientfd[id], "505", "HTTP Version not supported",
                    "HTTP/1.0 is not supported by Liso server", *is_closed);  
        goto Done;
    }

    // parse uri (get filename and parameters if any)
    parse_uri(context);
   
    // parse request headers 
    if (parse_requestheaders(id, p, context, is_closed) < 0) goto Done;

    // for POST, parse request body
    if (!strcasecmp(context->method, "POST"))
        if (parse_requestbody(id, p, context, is_closed) < 0) goto Done;

    // send response 
    if (!strcasecmp(context->method, "GET"))
        serve_get(p->clientfd[id], context, is_closed); 
    else if (!strcasecmp(context->method, "POST")) 
        serve_post(p->clientfd[id], context, is_closed);
    else if (!strcasecmp(context->method, "HEAD")) 
        serve_head(p->clientfd[id], context, is_closed);

    Done:
    free(context); 
    Log("End of processing request.");
}

/******************************************************************************
* subroutine: parse_requestline                                               *
* purpose:    parse the content of request line                               *
* parameters: id        - the index of the client in the pool                 *
*             p         - a pointer of the pool data structure                *
*             context   - a pointer refers to HTTP context                    *
*             is_closed - an indicator if the current transaction is closed   *
* return:     0 on success -1 on error                                        *
******************************************************************************/
int parse_requestline(int id, pool *p, HTTPContext *context, int *is_closed)
{
    char buf[MAX_LINE];

    memset(buf, 0, MAX_LINE); 

    if (rio_readlineb(&p->clientrio[id], buf, MAX_LINE) < 0)
    {
        *is_closed = 1;
        Log("Error: rio_readlineb error in process_request");
        serve_error(p->clientfd[id], "500", "Internal Server Error",
                    "The server encountered an unexpected condition.", *is_closed);
        return -1;
    }

    if (sscanf(buf, "%s %s %s", context->method, context->uri, context->version) < 3)
    {
        *is_closed = 1;
        Log("Info: Invalid request line");
        serve_error(p->clientfd[id], "400", "Bad Request",
                    "The request is not understood by the server", *is_closed);
        return -1;
    }

    return 0;
}

/******************************************************************************
* subroutine: parse_requestheaders                                            *
* purpose:    parse the content of request headers                            *
* parameters: id        - the index of the client in the pool                 *
*             p         - a pointer of the pool data structure                *
*             context   - a pointer refers to HTTP context                    *
*             is_closed - an indicator if the current transaction is closed   *
* return:     0 on success -1 on error                                        *
******************************************************************************/
int parse_requestheaders(int id, pool *p, HTTPContext *context, int *is_closed)
{
    int  ret, cnt = 0, has_contentlen = 0;
    char buf[MAX_LINE], header[MIN_LINE], data[MIN_LINE];
    
    context->content_len = -1; 

    do
    {   
        if ((ret = rio_readlineb(&p->clientrio[id], buf, MAX_LINE)) < 0)
            break;

        cnt += ret;

        // if request header is larger than 8196, reject request
        if (cnt > MAX_LINE)
        {
            *is_closed = 1;
            serve_error(p->clientfd[id], "400", "Bad Request",
                       "Request header too long.", *is_closed);
            return -1;
        }
        
        if (strstr(buf, "Connection: close")) *is_closed = 1;

        if (strstr(buf, "Content-Length")) 
        {
            has_contentlen = 1;
            if (sscanf(buf, "%s %s", header, data) > 0)
                context->content_len = (int)strtol(data, (char**)NULL, 10); 
            Log(data);
        }  

    } while(strcmp(buf, "\r\n"));

    if ((!has_contentlen) && (!strcasecmp(context->method, "POST")))
    {
        serve_error(p->clientfd[id], "411", "Length Required",
                       "Content-Length is required.", *is_closed);
        return -1;
    }

    return 0;
}

/******************************************************************************
* subroutine: parse_requestbody                                               *
* purpose:    parse the content of request body (for POST)                    *
* parameters: id        - the index of the client in the pool                 *
*             p         - a pointer of the pool data structure                *
*             context   - a pointer refers to HTTP context                    *
*             is_closed - an indicator if the current transaction is closed   *
* return:     0 on success -1 on error                                        *
******************************************************************************/
int parse_requestbody(int id, pool *p, HTTPContext *context, int *is_closed)
{
    ///TODO
    return 0;
}
/******************************************************************************
* subroutine: parse_uri                                                       *
* purpose:    to parse filename and CGI arguments from uri                    *
* parameters: context - a pointer of the HTTP context data structure          *
* return:     none                                                            *
******************************************************************************/
void parse_uri(HTTPContext *context)
{
    char *ptr;

    ///TODO check HTTP://
    // initialize filename path
    strcpy(context->filename, STATE.www_path);

    // parse uri
    if (!strstr(context->uri, "cgi-bin"))  // static content
    {
        context->is_static = 1;
        strcat(context->filename, context->uri);
        if (context->uri[strlen(context->uri)-1] == '/')
            strcat(context->filename, "index.html");
    }
    else
    {                             // dynamic content
        ptr = index(context->uri, '?');
        if (ptr)
        {
            strcpy(context->cgiargs, ptr+1);
            *ptr = '\0'; ///TODO what is this for?
        }
        else
        {        
            strcpy(context->cgiargs, "");
        }
    }
}

/******************************************************************************
* subroutine: validate_file                                                   *
* purpose:    validate file existence and permisson                           *
* parameters: client_fd - client descriptor                                   *
*             context   - a pointer refers to HTTP context                    *
*             is_closed - an indicator if the current transaction is closed   *
* return:     0 on success -1 on error                                        *
******************************************************************************/
int validate_file(int client_fd, HTTPContext *context, int *is_closed)
{
    struct stat sbuf;

    // check file existence
    if (stat(context->filename, &sbuf) < 0)
    {
        serve_error(client_fd, "404", "Not Found",
                    "Server couldn't find this file", *is_closed);
        return -1;
    }

    // check file permission
    if ((!S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
        serve_error(client_fd, "403", "Forbidden",
                    "Server couldn't read this file", *is_closed);
        return -1;
    }

    return 0;
}

/******************************************************************************
* subroutine: serve_head                                                      *
* purpose:    return response header to client                                *
* parameters: client_fd - client descriptor                                   *
*             context   - a pointer refers to HTTP context                    *
*             is_closed - an indicator if the current transaction is closed   *
* return:     none                                                            *
******************************************************************************/
void serve_head(int client_fd, HTTPContext *context, int *is_closed)
{
    struct tm tm;
    struct stat sbuf;
    time_t now;
    char   buf[BUF_SIZE], filetype[MIN_LINE], tbuf[MIN_LINE], dbuf[MIN_LINE]; 

    if (validate_file(client_fd, context, is_closed) < 0) return;

    stat(context->filename, &sbuf);
    get_filetype(context->filename, filetype);

    // get time string
    tm = *gmtime(&sbuf.st_mtime);
    strftime(tbuf, MIN_LINE, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, MIN_LINE, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // send response headers to client
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sDate: %s\r\n", buf, dbuf);
    sprintf(buf, "%sServer: Liso/1.0\r\n", buf);
    if (is_closed) sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-Length: %ld\r\n", buf, sbuf.st_size);
    sprintf(buf, "%sContent-Type: %s\r\n", buf, filetype);
    sprintf(buf, "%sLast-Modified: %s\r\n\r\n", buf, tbuf);
    send(client_fd, buf, strlen(buf), 0);
}

/******************************************************************************
* subroutine: serve_body                                                      *
* purpose:    return response body to client                                  *
* parameters: client_fd - client descriptor                                   *
*             context   - a pointer refers to HTTP context                    *
*             is_closed - an indicator if the current transaction is closed   *
* return:     none                                                            *
******************************************************************************/
int serve_body(int client_fd, HTTPContext *context, int *is_closed)
{
    int fd, filesize;
    char *ptr;
    struct stat sbuf;
    
    if ((fd = open(context->filename, O_RDONLY, 0)) < 0)
    {
        Log("Error: Cann't open file");
        return -1; ///TODO what error code here should be?
    }

    stat(context->filename, &sbuf);

    filesize = sbuf.st_size;
    ptr = mmap(0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    send(client_fd, ptr, filesize, 0); 
    munmap(ptr, filesize);

    return 0;
}

/******************************************************************************
* subroutine: serve_get                                                       *
* purpose:    return response for GET request                                 *
* parameters: client_fd - client descriptor                                   *
*             context   - a pointer refers to HTTP context                    *
*             is_closed - an indicator if the current transaction is closed   *
* return:     none                                                            *
******************************************************************************/
void serve_get(int client_fd, HTTPContext *context, int *is_closed)
{

    serve_head(client_fd, context, is_closed);
    serve_body(client_fd, context, is_closed);

}

/******************************************************************************
* subroutine: serve_post                                                      *
* purpose:    return response for POST request                                *
* parameters: client_fd - client descriptor                                   *
*             context   - a pointer refers to HTTP context                    *
*             is_closed - an indicator if the current transaction is closed   *
* return:     none                                                            *
******************************************************************************/
void serve_post(int client_fd, HTTPContext *context, int *is_closed)
{
    struct tm tm;
    struct stat sbuf;
    time_t now;
    char   buf[BUF_SIZE], dbuf[MIN_LINE]; 

    // check file existence
    if (stat(context->filename, &sbuf) == 0)
    {
        serve_get(client_fd, context, is_closed);
        return;
    }

    // get time string
    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, MIN_LINE, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // send response headers to client
    sprintf(buf, "HTTP/1.1 204 No Content\r\n");
    sprintf(buf, "%sDate: %s\r\n", buf, dbuf);
    sprintf(buf, "%sServer: Liso/1.0\r\n", buf);
    if (is_closed) sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-Length: 0\r\n", buf);
    sprintf(buf, "%sContent-Type: text/html\r\n", buf);
    send(client_fd, buf, strlen(buf), 0);
}

/******************************************************************************
* subroutine: get_filetype                                                    *
* purpose:    find filetype by filename extension                             *
* parameters: filename: the requested filename                               *
*             filetype: a pointer to return filetype result                   *
* return:     none                                                            *
******************************************************************************/
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".css"))
        strcpy(filetype, "text/css");
    else if (strstr(filename, ".js"))
        strcpy(filetype, "application/javascript");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
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
*             is_closed - an indicate if sending 'Connection: close' back     *
* return:     none                                                            *
******************************************************************************/
void serve_error(int client_fd, char *errnum, char *shortmsg, char *longmsg, 
                 int is_closed) {
    struct tm tm;
    time_t now;
    char buf[MAX_LINE], body[MAX_LINE], dbuf[MIN_LINE];

    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, MIN_LINE, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // build HTTP response body
    sprintf(body, "<html><title>Lisod Error</title>");
    sprintf(body, "%s<body>\r\n", body);
    sprintf(body, "%sError %s -- %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<br><p>%s</p></body></html>\r\n", body, longmsg);

    // print HTTP response
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    sprintf(buf, "%sDate: %s\r\n", buf, dbuf);
    sprintf(buf, "%sServer: Liso/1.0\r\n", buf);
    if (is_closed) sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-type: text/html\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, (int)strlen(body));
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
        Log("Error: failed closing socket.");
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

void signal_handler(int sig)
{
    switch(sig)
    {
        case SIGHUP:
            Log("hangup signal catched");
            break; // rehash the server;
        case SIGTERM:
            KEEPON = 0;
        default:
            break; // unhandled signals
    }
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
