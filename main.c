#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>

#include "ftpservice.h"

#define PORT 2121
#define MAX_CLIENT_CONNECTIONS 4

client_session_t sessions[MAX_CLIENT_CONNECTIONS];

char *root_directory;
int hostip_octets[4];
cmd_map_t cmd_map[NUM_CMDS] = {
    {"USER", CMD_USER}, {"PASS", CMD_PASS}, {"QUIT", CMD_QUIT},
    {"SYST", CMD_SYST}, {"PWD", CMD_PWD},   {"CWD", CMD_CWD},
    {"CDUP", CMD_CDUP}, {"TYPE", CMD_TYPE}, {"MODE", CMD_MODE},
    {"STRU", CMD_STRU}, {"RETR", CMD_RETR}, {"PORT", CMD_PORT},
    {"PASV", CMD_PASV}, {"LIST", CMD_LIST}, {"NLST", CMD_NLST}};

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

int main(int argc, char **argv) {
    // Set root directory
    root_directory = getenv("FTP_ROOT");
    if (root_directory == NULL) {
        perror("Missing FTP_ROOT env variable\n");
        return 1;
    }
    DIR *dir = opendir(root_directory);
    if (dir == NULL) {
        perror("FTP_ROOT folder does not exist\n");
        return 1;
    }
    closedir(dir);

    int serverfd = open_port(PORT);
    set_hostip();
    printf("Listening on port %d\n", PORT);

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
