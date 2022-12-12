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
 * @param sock return socket
 * @return 1 if sock successfully binded to a port; else 0
 */
int open_port(int port, socket_t *sock) {
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd == -1) {
        printf("Socket creation failed\n");
        return 0;
    }
    sock->addr.sin_family = AF_INET;
    sock->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (port == 0) {
        // Try any available port
        int try_port;
        for (try_port = MIN_PORT; try_port <= MAX_PORT; try_port++) {
            sock->addr.sin_port = htons(try_port);
            if (bind(sock->fd,
                     (struct sockaddr *) &sock->addr,
                     sizeof(struct sockaddr_in)) >= 0) {
                break;
            }
        }
        if (try_port > MAX_PORT) {
            printf("Socket bind failed\n");
            return 0;
        }
    } else {
        sock->addr.sin_port = htons(port);
        if (bind(sock->fd,
                 (struct sockaddr *) &sock->addr,
                 sizeof(struct sockaddr_in)) < 0) {
            printf("Socket bind to %d failed\n", port);
            return 0;
        }
    }
    listen(sock->fd, 5);
    return 1;
}

/**
 * Connects socket to given host and port
 *
 * @param ipaddr
 * @param sock
 * @return 1 if sock successfully connected to host; else 0
 */
int connect_to_host(char *host, int port, socket_t *sock) {
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd == -1) {
        printf("Socket creation failed\n");
        return 0;
    }
    sock->addr.sin_family = AF_INET;
    sock->addr.sin_addr.s_addr = inet_addr(host);
    sock->addr.sin_port = htons(port);

    if (connect(sock->fd,
                (struct sockaddr *) &sock->addr,
                sizeof(sock->addr)) != 0) {
        printf("Socket connect failed\n");
        return 0;
    }
    return 1;
}

/**
 * Get port number from given sock
 *
 * @param sock socket
 * @return port number
 */
int get_socket_port(socket_t *sock) {
    return ntohs(sock->addr.sin_port);
}

/**
 * Returns 1 if port is within a valid range; else 0
 *
 * @param port port number
 * @return int
 */
int is_valid_port(int port) {
    return port >= MIN_PORT && port <= MAX_PORT;
}
