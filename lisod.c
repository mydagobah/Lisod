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
*              1. echo: simply write back anything sent to it by connected     *
*                 clients.                                                     *
*              2. support connections from multiple clients                    *
*              3. a default log file 'lisod.log' at the same directory         *
*                                                                              *
*                                                                              *
* Authors: Wenjun Zhang <wenjunzh@andrew.cmu.edu>,                             *
*                                                                              *
* Usage:   ./lisod <HTTP port> <log file>                                      *
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

    init();
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

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
       pool.ready_set = pool.read_set;
       pool.nready = select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

       if (pool.nready < 0)
       {
           close(sock);
           fprintf(stderr, "Error select connections.\n");
           return EXIT_FAILURE;
       }

       // if sock descriptor ready, add new client to pool
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
* purpose:    perform all of the initialization                               *
* parameters: none                                                            *
* return:     none                                                            *
******************************************************************************/
void init()
{
    STATE.logfile = log_open();
    STATE.port = 9999;
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

    // Initially, sock is the only member of select read set
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
            echo(connfd);
        }
    }
}

/******************************************************************************
* subroutine: echo                                                            *
* purpose:    return any message client send to server                        *
* parameters: connfd - connection descriptor                                  *
* return:     0 on success, -1 on failure                                     *
******************************************************************************/
int echo(int connfd)
{
    ssize_t readret = 0;
    char buf[BUF_SIZE];

    if ((readret = recv(connfd, buf, BUF_SIZE, 0)) > 1)
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
    return 0;
}

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

void clean()
{
    fclose(STATE.logfile);
    close_socket(STATE.sock);
}
