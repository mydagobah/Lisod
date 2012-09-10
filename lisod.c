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

void init()
{
    STATE.logfile = log_open();
    STATE.port = 9999;
}

int main(int argc, char* argv[])
{
    init();

    Log("Start Liso server");

    int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];

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
    Log("call socket done!");

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }
    Log("call bind done!");

    if (listen(sock, MAX_CONN))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }
    Log("call listen done!");

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
       cli_size = sizeof(cli_addr);
       if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                                 &cli_size)) == -1)
       {
           close(sock);
           fprintf(stderr, "Error accepting connection.\n");
           return EXIT_FAILURE;
       }
       Log("accept client");

       readret = 0;

       while((readret = recv(client_sock, buf, BUF_SIZE, 0)) > 1)
       {
           if (send(client_sock, buf, readret, 0) != readret)
           {
               close_socket(client_sock);
               close_socket(sock);
               fprintf(stderr, "Error sending to client.\n");
               return EXIT_FAILURE;
           }
           buf[readret-1] = '\0';
           Log(buf);

           memset(buf, 0, BUF_SIZE);
       }

       if (readret == -1)
       {
           close_socket(client_sock);
           close_socket(sock);
           fprintf(stderr, "Error reading from client socket.\n");
           return EXIT_FAILURE;
       }

       if (close_socket(client_sock))
       {
           close_socket(sock);
           fprintf(stderr, "Error closing client socket.\n");
           return EXIT_FAILURE;
       }
    }

    close_socket(sock);
    clean();

    return EXIT_SUCCESS;
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
}
