#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include "log.h"

#define BUF_SIZE 4096
#define ECHO_PORT 9999
#define MAX_CONN 1024

void init();
void clean();
int close_socket(int sock);
