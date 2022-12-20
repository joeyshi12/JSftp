#ifndef __TCPSERVER_H__
#define __TCPSERVER_H__

#include <arpa/inet.h>
#include <netinet/in.h>

#define FTP_PORT 21
#define MIN_PORT 1024
#define MAX_PORT 65535

int open_port(int port);

int get_socket_port(int fd);

#endif
