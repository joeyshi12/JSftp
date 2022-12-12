#ifndef __TCPSERVER_H__
#define __TCPSERVER_H__

#include <arpa/inet.h>
#include <netinet/in.h>

#define FTP_PORT 21
#define MIN_PORT 1024
#define MAX_PORT 65535

typedef struct socket_s {
    int fd;
    struct sockaddr_in addr;
    socklen_t addrlen;
} socket_t;

int open_port(int port, socket_t *sock);

int connect_to_host(char *ipaddr, int port, socket_t *sock);

int get_socket_port(socket_t *sock);

int is_valid_port(int port);

#endif
