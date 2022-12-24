/**
 * @file JSftp.c
 * Entry point for FTP server program: creates server socket listening to a
 * given port and accepts connections
 *
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftpservice.h"
#include "usage.h"

#define MAX_CLIENT_CONNECTIONS 8

client_session_t sessions[MAX_CLIENT_CONNECTIONS];

char root_directory[PATH_LEN];
uint8_t hostip_octets[4];
cmd_map_t cmd_map[NUM_CMDS] = {
    {"USER", CMD_USER}, {"PASS", CMD_PASS}, {"QUIT", CMD_QUIT},
    {"SYST", CMD_SYST}, {"PWD", CMD_PWD},   {"CWD", CMD_CWD},
    {"CDUP", CMD_CDUP}, {"TYPE", CMD_TYPE}, {"MODE", CMD_MODE},
    {"STRU", CMD_STRU}, {"RETR", CMD_RETR}, {"PORT", CMD_PORT},
    {"PASV", CMD_PASV}, {"LIST", CMD_LIST}, {"NLST", CMD_NLST}};

/**
 * Get port number from str
 *
 * @param str
 * @param port output port number
 * @return 1 if str can be converted to port number; else 0
 */
int get_port(char *str, int *port) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) {
            return 0;
        }
    }
    *port = atoi(str);
    return 1;
}

/**
 * Finds IP address of host machine and sets host IP octets
 * https://stackoverflow.com/questions/212528/how-can-i-get-the-ip-address-of-a-linux-machine
 *
 */
void set_hostip() {
    struct ifaddrs *if_addrs = NULL;
    void *sin_addr = NULL;
    char address[INET_ADDRSTRLEN];

    getifaddrs(&if_addrs);
    for (struct ifaddrs *ifa = if_addrs; ifa != NULL; ifa = ifa->ifa_next) {
        // Check it is an IPv4 address
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        sin_addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        inet_ntop(AF_INET, sin_addr, address, INET_ADDRSTRLEN);
        // Skip localhost
        if (strncmp("127", address, 3) == 0) {
            continue;
        }
        char *token = strtok(address, ".");
        for (int i = 0; i < 4; i++) {
            if (token == NULL) {
                break;
            }
            hostip_octets[i] = atoi(token);
            token = strtok(NULL, ".");
        }
        break;
    }

    if (if_addrs != NULL) {
        freeifaddrs(if_addrs);
    }
}

int next_session() {
    int i;
    for (i = 0; i < MAX_CLIENT_CONNECTIONS; i++) {
        if (sessions[i].state == STATE_EXITED) {
            pthread_join(sessions[i].session_thread, NULL);
            sessions[i].state = STATE_OPEN;
        }
    }
    for (i = 0; i < MAX_CLIENT_CONNECTIONS; i++) {
        if (sessions[i].state == STATE_OPEN) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    int port;
    if (argc != 2 || get_port(argv[1], &port) == -1) {
        usage(argv[0]);
        return 0;
    }

    // Set current working directory as program root directory
    getcwd(root_directory, sizeof(root_directory));

    // Set IP of host machine
    set_hostip();

    // Start server on port
    int serverfd = open_port(port);
    if (serverfd == -1) {
        return 0;
    }
    printf("Listening on %d.%d.%d.%d:%d\n",
           hostip_octets[0],
           hostip_octets[1],
           hostip_octets[2],
           hostip_octets[3],
           port);

    int clientfd;
    int sessionid;
    client_session_t *session;
    struct sockaddr_in sin;
    socklen_t addrlen = sizeof(sin);
    while (1) {
        clientfd = accept(serverfd, (struct sockaddr *)&sin, &addrlen);
        sessionid = next_session();
        if (sessionid == -1) {
            printf("Max capacity reached: cannot accept client\n");
            close(clientfd);
            continue;
        }
        session = &sessions[sessionid];
        session->clientfd = clientfd;
        session->state = STATE_AWAITING_USER;
        pthread_create(&session->session_thread, NULL, handle_session, (void *)session);
        printf("FTP session opened (connect).\n");
    }
    return 0;
}
