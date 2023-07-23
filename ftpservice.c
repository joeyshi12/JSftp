/**
 * @file ftpservice.c
 * Functions for handling FTP commands from connected users
 *
 * Public functions:
 * - handle_session
 *
 */

#include "ftpservice.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "dir.h"

/**
 * Main loop for an FTP session of a user with the given control connection fd
 *
 * @param session_data session bytes for control connection
 * @return NULL
 */
void *handle_session(void *session_data) {
    client_session_t *session = session_data;
    close_connection(&session->data_connection);

    // Respond connection successful
    dprintf(session->clientfd, "220 (JSftp 1.0)\r\n");

    // Session loop
    char recvbuf[1024];
    char *cmdstr;
    int argc;
    char *args[MAX_NUM_ARGS];
    int recvsize;
    char *saveptr = NULL;  // for thread-safe strtok_r
    while (1) {
        recvsize = read(session->clientfd, &recvbuf, sizeof(recvbuf));
        if (recvsize <= 0) {  // client ctrl-c
            break;
        }
        if (recvbuf[0] == '\r' || recvbuf[0] == '\n') {
            continue;
        }
        if (recvsize > 1023) {
            recvsize = 1023;
        }
        recvbuf[recvsize] = '\0';
        printf("<-- %s", recvbuf);

        // Parse command string
        cmdstr = trimstr(strtok_r(recvbuf, " ", &saveptr));
        for (argc = 0; argc < MAX_NUM_ARGS; argc++) {
            args[argc] = trimstr(strtok_r(NULL, " ", &saveptr));
            if (args[argc] == NULL) {
                break;
            }
        }

        // Execute command
        if (execute_cmd(to_cmd(cmdstr), argc, args, session)) {
            break;
        }
    }

    // End session
    close(session->clientfd);
    close_connection(&session->data_connection);
    printf("FTP session closed (disconnect).\r\n");
    session->state = STATE_EXITED;
    return NULL;
}

/**
 * Executes given FTP command requested by client
 *
 * @param cmd command type
 * @param argc length of args
 * @param args arguments for command
 * @param session session of current client session
 * @return 1 if user session should be closed; else 0
 */
int execute_cmd(cmd_t cmd, int argc, char *args[], client_session_t *session) {
    if (cmd != CMD_USER && cmd != CMD_PASS && cmd != CMD_QUIT &&
        session->state == STATE_AWAITING_USER) {
        dprintf(session->clientfd, "530 Please login with USER.\r\n");
        return 0;
    }
    switch (cmd) {
        case (CMD_USER):
            return handle_user(session, argc, args);
        case (CMD_PASS):
            return handle_pass(session, argc);
        case (CMD_QUIT):
            dprintf(session->clientfd, "221 Goodbye.\r\n");
            return 1;
        case (CMD_SYST):
            dprintf(session->clientfd, "215 UNIX Type: L8\r\n");
            return 0;
        case (CMD_PWD):
            return handle_pwd(session, argc);
        case (CMD_CWD):
            return handle_cwd(session, argc, args);
        case (CMD_CDUP):
            return handle_cdup(session, argc);
        case (CMD_TYPE):
            return handle_type(session, argc, args);
        case (CMD_MODE):
            return handle_mode(session, argc, args);
        case (CMD_STRU):
            return handle_stru(session, argc, args);
        case (CMD_RETR):
            return handle_retr(session, argc, args);
        case (CMD_PORT):
            return handle_port(session, argc, args);
        case (CMD_PASV):
            return handle_pasv(session, argc);
        case (CMD_LIST):
            return handle_nlst(session, argc);
        case (CMD_NLST):
            return handle_nlst(session, argc);
        default:
            dprintf(session->clientfd, "500 Unknown command.\r\n");
            return 0;
    }
}

/**
 * Sets valid user session to 1 in session session if username arg is cs317
 *
 * @param argc
 * @param args
 * @param session
 * @return 0
 */
int handle_user(client_session_t *session, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    if (session->state == STATE_ACTIVE) {
        dprintf(session->clientfd, "530 Can't change from %s.\r\n", USER);
        return 0;
    }
    if (argc != 1 || strcmp(args[0], USER) != 0) {
        session->state = STATE_AWAITING_USER;
        dprintf(session->clientfd, "530 This FTP server is %s only.\r\n", USER);
        return 0;
    }

    // Set current working directory for client
    strcpy(session->cwd, root_directory);
    session->state = STATE_AWAITING_PASS;
    dprintf(session->clientfd, "331 Please specify the password.\r\n");
    return 0;
}

/**
 * Sets login session to 1 if user is valid
 *
 * @param session
 * @param argc
 * @return int
 */
int handle_pass(client_session_t *session, int argc) {
    if (argc != 1) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    if (session->state == STATE_ACTIVE) {
        dprintf(session->clientfd, "230 Already logged in.\r\n");
        return 0;
    }
    if (session->state != STATE_AWAITING_PASS) {
        dprintf(session->clientfd, "503 Login with USER first.\r\n");
        return 0;
    }
    session->state = STATE_ACTIVE;
    printf("User logged in.\r\n");
    dprintf(session->clientfd, "230 Login successful.\r\n");
    return 0;
}


/**
 * Prints current working directory to client fd
 *
 * @param session
 * @param argc
 * @return int
 */
int handle_pwd(client_session_t *session, int argc) {
    if (argc != 0) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    if (strcmp(session->cwd, root_directory) == 0) {
        dprintf(session->clientfd, "257 \"/\"\r\n");
    } else {
        dprintf(session->clientfd, "257 \"%s\"\r\n", &session->cwd[strlen(root_directory)]);
    }
    return 0;
}

/**
 * Changes CWD in user session to the given relative path arg[0]
 *
 * @param session
 * @param argc
 * @param args
 * @return 0
 */
int handle_cwd(client_session_t *session, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    char dirpath[PATH_LEN];
    if (to_absolute_path(args[0], session->cwd, dirpath) == 0) {
        dprintf(session->clientfd, "550 Failed to change directory.\r\n");
        return 0;
    }
    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        dprintf(session->clientfd, "550 No such directory.\r\n");
        return 0;
    }
    closedir(dir);
    strcpy(session->cwd, dirpath);
    printf("CWD %s 250\r\n", session->cwd);
    dprintf(session->clientfd, "250 Working directory changed.\r\n");
    return 0;
}

/**
 * Sets CWD for client session to parent directory of CWD if CWD is not root
 *
 * @param session
 * @param argc
 * @return 0
 */
int handle_cdup(client_session_t *session, int argc) {
    if (argc != 0) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    if (strcmp(session->cwd, root_directory) == 0) {
        dprintf(session->clientfd, "550 Directory not accessible.\r\n");
        return 0;
    }

    // Set last occurrance of '/' to '\0'
    char *end = session->cwd + strlen(session->cwd) - 1;
    while ((unsigned char)*end != '/') end--;
    *end = '\0';

    printf("CDUP 250, CWD=%s\r\n", session->cwd);
    dprintf(session->clientfd, "250 Working directory changed.\r\n");
    return 0;
}

/**
 * Sends status 200 if type arg is a valid type; else sends error status code
 *
 * @param session
 * @param argc
 * @param args
 * @return 0
 */
int handle_type(client_session_t *session, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    char *type = args[0];
    if (strcasecmp(type, "A") == 0) {
        dprintf(session->clientfd, "200 Set to ASCII type.\r\n");
    } else if (strcasecmp(type, "I") == 0) {
        dprintf(session->clientfd, "200 Set to Image type.\r\n");
    } else {
        dprintf(session->clientfd,
                "504 Unsupported type-code. Only type A and I are allowed.\r\n");
    }
    return 0;
}

/**
 * Sends status 200 if type arg is a valid mode; else sends error status code
 *
 * @param session
 * @param argc
 * @param args
 * @return 0
 */
int handle_mode(client_session_t *session, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    char *mode = args[0];
    if (strcasecmp(mode, "S") == 0) {
        dprintf(session->clientfd, "200 Set to streaming mode.\r\n");
    } else {
        dprintf(session->clientfd, "504 Unsupported mode-code. Only type S is allowed.\r\n");
    }
    return 0;
}

/**
 * Sends status 200 if file_structure arg is a valid file structure;
 * else sends error code
 *
 * @param session
 * @param argc
 * @param args
 * @return 0
 */
int handle_stru(client_session_t *session, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    char *file_structure = args[0];
    if (strcasecmp(file_structure, "F") == 0) {
        dprintf(session->clientfd, "200 Set to file structure.\r\n");
    } else {
        dprintf(session->clientfd,
                "504 Unsupported structure-code. Only type F is allowed.\r\n");
    }
    return 0;
}

/**
 * Writes bytes from user input filename to DTP client fd if file exists
 * https://idiotdeveloper.com/file-transfer-using-tcp-socket-in-c/
 *
 * @param session
 * @param argc
 * @param args
 * @return 0
 */
int handle_retr(client_session_t *session, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    connection_t *connection = &session->data_connection;
    if (!connection->awaiting_client && connection->clientfd == -1) {
        dprintf(session->clientfd, "425 Use PASV first.\r\n");
        return 0;
    }
    // if (connection->awaiting_client) {
    //     dprintf(session->clientfd,
    //             "425 Data connection has not been established.\r\n");
    //     return 0;
    // }
    pthread_join(connection->accept_client_t, NULL);

    char filepath[PATH_LEN];
    if (to_absolute_path(args[0], session->cwd, filepath) == 0) {
        dprintf(session->clientfd, "550 File path not allowed.\r\n");
        return 0;
    }
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        dprintf(session->clientfd, "550 File does not exist.\r\n");
        return 0;
    }

    dprintf(session->clientfd, "150 Opening data connection for %s.\r\n", filepath);
    char buffer[1024];
    size_t totalbytes = 0;
    size_t bytesread;
    size_t bufoffset;
    size_t byteswrote;
    while ((bytesread = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        bufoffset = 0;
        while (bufoffset < bytesread) {
            byteswrote = write(
                connection->clientfd,
                buffer + bufoffset,
                bytesread - bufoffset
            );
            if (byteswrote <= 0) {
                dprintf(session->clientfd, "550 Could not send file.\r\n");
                close_connection(connection);
                return 0;
            }
            bufoffset += byteswrote;
        }
        totalbytes += bytesread;
    }

    fclose(file);
    file = NULL;
    printf("RETR %s completed with %u bytes sent.\r\n",
            filepath, (unsigned) totalbytes);
    dprintf(session->clientfd, "226 Transfer complete.\r\n");
    close_connection(connection);
    return 0;
}

/**
 * Initializes new DTP socket to listen for clients in session session
 *
 * @param session
 * @param argc
 * @return 0
 */
int handle_pasv(client_session_t *session, int argc) {
    if (argc != 0) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    connection_t *connection = &session->data_connection;
    if (connection->awaiting_client || connection->clientfd != -1) {
        close_connection(connection);
    }
    open_passive_port(session);
    int port = get_socket_port(connection->passivefd);
    dprintf(session->clientfd,
            "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
            hostip_octets[0], hostip_octets[1], hostip_octets[2],
            hostip_octets[3], port >> 8, port & 0xff);
    return 0;
}

/**
 * Lists directory entries in CWD to DTP client fd
 *
 * @param session
 * @param argc
 * @return 0
 */
int handle_nlst(client_session_t *session, int argc) {
    if (argc != 0) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    connection_t *connection = &session->data_connection;
    if (!connection->awaiting_client && connection->clientfd == -1) {
        dprintf(session->clientfd, "425 Use PASV first.\r\n");
        return 0;
    }

    // if (connection->awaiting_client) {
    //     dprintf(session->clientfd, "425 Data connection has not been established.\r\n");
    //     return 0;
    // }
    pthread_join(connection->accept_client_t, NULL);

    dprintf(session->clientfd, "150 Here comes the directory listing.\r\n");
    listFiles(connection->clientfd, session->cwd);
    char msg[] = "226 Directory send OK.\r\n";
    send(session->clientfd, msg, sizeof(msg), MSG_NOSIGNAL);
    close_connection(connection);
    return 0;
}

/**
 * Binds socket to data port
 *
 * @param session
 * @param argc
 * @param args
 * @return 0
 */
int handle_port(client_session_t *session, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(session->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    char *saveptr = NULL;
    char *tokens[8];
    int num_tokens;
    tokens[0] = trimstr(strtok_r(args[0], ",", &saveptr));
    for (num_tokens = 1; num_tokens < 8; num_tokens++) {
        if ((tokens[num_tokens] = trimstr(strtok_r(NULL, ",", &saveptr))) == NULL) {
            break;
        }
    }
    if (num_tokens != 6) {
        dprintf(session->clientfd, "500 Illegal PORT command.\r\n");
        return 0;
    }
    char ipaddr[256];
    strcpy(ipaddr, tokens[0]);
    for (int i = 1; i < 4; i++) {
        strcat(ipaddr, ".");
        strcat(ipaddr, tokens[i]);
    }
    int port = (atoi(tokens[4]) << 8) + atoi(tokens[5]);
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd < 0) {
        printf("Socket creation error\n");
        return 0;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (inet_pton(AF_INET, ipaddr, &sin.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        return 0;
    }

    int passivefd = connect(clientfd, (struct sockaddr *)&sin, sizeof(sin));
    if (passivefd < 0) {
        printf("Connect failed\n");
        return 0;
    }

    session->data_connection.passivefd = passivefd;
    session->data_connection.clientfd = clientfd;
    session->data_connection.awaiting_client = 0;
    dprintf(session->clientfd, "200 Connect successful.\r\n");
    return 0;
}

/**
 * Exposes a new port for a DTP connection for a given session session
 *
 * @param session session session
 * @return 1 on successful DTP socket bind; else 0
 */
int open_passive_port(client_session_t *session) {
    connection_t *connection = &session->data_connection;
    connection->passivefd = open_port(0);
    if (connection->passivefd == -1) {
        return 0;
    }
    connection->clientfd = -1;
    connection->awaiting_client = 1;
    pthread_create(
        &connection->accept_client_t,
        NULL,
        accept_data_client,
        (void *)session
    );
    return 1;
}

/**
 * Awaits DTP client to accept or times out if client does not connect
 * within DTP_TIMEOUT_SECONDS
 *
 * @param session_data bytes representing client control session session
 * @return NULL
 */
void *accept_data_client(void *session_data) {
    client_session_t *session = session_data;
    connection_t *connection = &session->data_connection;

    // https://linux.die.net/man/2/select
    struct timeval tv;
    fd_set readfds;
    tv.tv_sec = DTP_TIMEOUT_SECONDS;
    tv.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(connection->passivefd, &readfds);
    if (select(connection->passivefd + 1, &readfds, NULL, NULL, &tv) == 0) {
        dprintf(session->clientfd, "421 Timeout.\r\n");
        close_connection(connection);
        return NULL;
    }

    struct sockaddr_in sin;
    socklen_t addrlen = sizeof(sin);
    connection->clientfd = accept(connection->passivefd, (struct sockaddr *) &sin, &addrlen);
    if (connection->clientfd < 0) {
        dprintf(session->clientfd, "425 Could not open data connection.\r\n");
        close_connection(connection);
        return NULL;
    }

    connection->awaiting_client = 0;
    return NULL;
}

/**
 * Maps a given str to command type
 *
 * @param str
 * @return cmd_t of given string in cmd_map; returns CMD_INVALID if no match
 */
cmd_t to_cmd(char *str) {
    if (str == NULL) {
        return CMD_INVALID;
    }
    for (int i = 0; i < NUM_CMDS; i++) {
        cmd_map_t cmd_pair = cmd_map[i];
        if (strcasecmp(str, cmd_pair.cmd_str) == 0) {
            return cmd_pair.cmd;
        }
    }
    return CMD_INVALID;
}

/**
 * Close fds of connection and sets connection fields to empty values
 *
 * @param connection
 */
void close_connection(connection_t *connection) {
    if (connection->clientfd != -1) {
        close(connection->clientfd);
        connection->clientfd = -1;
    }
    if (connection->passivefd != -1) {
        close(connection->passivefd);
        connection->passivefd = -1;
    }
    if (connection->awaiting_client) {
        pthread_cancel(connection->accept_client_t);
        pthread_join(connection->accept_client_t, NULL);
    }
    connection->awaiting_client = 0;
}

/**
 * Converts a given relative path to absolute path relative to root_directory
 *
 * @param relpath relative path
 * @param cwd current working directory
 * @param outpath return absolute path
 * @return 1 if path is accessible; else 0
 */
int to_absolute_path(char *relpath, char cwd[], char outpath[]) {
    if (strstr(relpath, "./") != NULL || strstr(relpath, "../") != NULL ||
        strcmp(relpath, "..") == 0) {
        return 0;
    }
    char absolute_path[PATH_LEN];
    if (relpath[0] == '/') {
        // Prevent CWD to parent
        if (strcmp(relpath, "/..") == 0) {
            return 0;
        }
        strcpy(absolute_path, root_directory);
        strcat(absolute_path, relpath);
    } else {
        strcpy(absolute_path, cwd);
        strcat(absolute_path, "/");
        strcat(absolute_path, relpath);
    }
    // Set canonicalized absolute pathname in outpath
    realpath(absolute_path, outpath);
    return 1;
}

char *trimstr(char *str) {
    if (str == NULL) {
        return str;
    }
    char *end;
    while (istrimchar(*str)) str++;
    if (*str == 0) {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && istrimchar(*end)) end--;
    end[1] = '\0';
    return str;
}

int istrimchar(unsigned char chr) {
    return isspace(chr) || chr == '\r' || chr == '\n';
}
