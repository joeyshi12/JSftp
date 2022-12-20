/**
 * @file tcpserver.c
 * Utility functions for initializing the TCP server socket
 *
 * Public functions:
 * - open_port
 * - get_socket_port
 * - is_valid_port
 *
 */

#include "tcpserver.h"

#include <stdio.h>

/**
 * Binds a new socket to given port if port is nonzero;
 * else binds new socket to smallest available port
 *
 * @param port port number
 * @return socket fd if socket is successfully listening on given port; else -1
 */
int open_port(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        printf("Socket creation failed\n");
        return -1;
    }
    int options = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (void *)&options, sizeof(options))) {
        printf("Setting socket options failed\n");
        return -1;
    }
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        printf("Socket bind to %d failed\n", port);
        return -1;
    }
    listen(fd, 5);
    return fd;
}

/**
 * Gets port number from given socket fd
 *
 * @param fd socket fd
 * @return port number if possible; else -1
 */
int get_socket_port(int fd) {
    struct sockaddr_in sin;
    socklen_t len;
    if (getsockname(fd, (struct sockaddr *)&sin, &len) == -1) {
        return -1;
    }
    return ntohs(sin.sin_port);
}
