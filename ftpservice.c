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
 * @param clientfd fd for control connection
 * @return NULL
 */
void *handle_session(void *clientfd) {
    session_state_t state;
    state.clientfd = *(int *)clientfd;
    state.is_valid_user = 0;
    state.is_logged_in = 0;
    state.data_connection.clientfd = -1;
    state.data_connection.awaiting_client = 0;

    // Respond connection successful
    dprintf(state.clientfd, "220 (JSftp 1.0)\r\n");

    // Session loop
    char recvbuf[1024];
    char *cmdstr;
    int argc;
    char *args[MAX_NUM_ARGS];
    int recvsize;
    char *saveptr = NULL;  // for thread-safe strtok_r
    while (1) {
        recvsize = read(state.clientfd, &recvbuf, sizeof(recvbuf));
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
        if (execute_cmd(to_cmd(cmdstr), argc, args, &state)) {
            break;
        }
    }

    // End session
    close(state.clientfd);
    close_connection(&state.data_connection);
    printf("FTP session closed (disconnect).\r\n");
    return NULL;
}

/**
 * Executes given FTP command requested by client
 *
 * @param cmd command type
 * @param argc length of args
 * @param args arguments for command
 * @param state state of current client session
 * @return 1 if user session should be closed; else 0
 */
int execute_cmd(cmd_t cmd, int argc, char *args[], session_state_t *state) {
    if (cmd != CMD_USER && cmd != CMD_PASS && cmd != CMD_QUIT &&
        (!state->is_valid_user || !state->is_logged_in)) {
        dprintf(state->clientfd, "530 Please login with USER.\r\n");
        return 0;
    }
    switch (cmd) {
        case (CMD_USER):
            return submit_user(state, argc, args);
        case (CMD_PASS):
            return submit_pass(state, argc);
        case (CMD_QUIT):
            dprintf(state->clientfd, "221 Goodbye.\r\n");
            return 1;
        case (CMD_SYST):
            dprintf(state->clientfd, "215 UNIX Type: L8\r\n");
            return 0;
        case (CMD_PWD):
            return pwd(state, argc);
        case (CMD_CWD):
            return cwd(state, argc, args);
        case (CMD_CDUP):
            return cdup(state, argc);
        case (CMD_TYPE):
            return set_type(state, argc, args);
        case (CMD_MODE):
            return set_mode(state, argc, args);
        case (CMD_STRU):
            return set_file_structure(state, argc, args);
        case (CMD_RETR):
            return retrieve_file(state, argc, args);
        case (CMD_PORT):
            return data_port(state, argc, args);
        case (CMD_PASV):
            return passive_mode(state, argc);
        case (CMD_LIST):
            return nlst(state, argc);
        case (CMD_NLST):
            return nlst(state, argc);
        default:
            dprintf(state->clientfd, "500 Unknown command.\r\n");
            return 0;
    }
}

/**
 * Sets valid user state to 1 in session state if username arg is cs317
 *
 * @param argc
 * @param args
 * @param state
 * @return 0
 */
int submit_user(session_state_t *state, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    if (state->is_logged_in) {
        dprintf(state->clientfd, "530 Can't change from %s.\r\n", USER);
        return 0;
    }
    if (argc != 1 || strcmp(args[0], USER) != 0) {
        state->is_valid_user = 0;
        dprintf(state->clientfd, "530 This FTP server is %s only.\r\n", USER);
        return 0;
    }

    // Set current working directory for client
    strcpy(state->cwd, root_directory);
    state->is_valid_user = 1;
    dprintf(state->clientfd, "331 Please specify the password.\r\n");
    return 0;
}

/**
 * Sets login state to 1 if user is valid
 *
 * @param state
 * @param argc
 * @return int
 */
int submit_pass(session_state_t *state, int argc) {
    if (argc != 1) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    if (state->is_logged_in) {
        dprintf(state->clientfd, "230 Already logged in.\r\n");
        return 0;
    }
    if (!state->is_valid_user) {
        dprintf(state->clientfd, "503 Login with USER first.\r\n");
        return 0;
    }
    state->is_logged_in = 1;
    printf("User logged in.\r\n");
    dprintf(state->clientfd, "230 Login successful.\r\n");
    return 0;
}


/**
 * Prints current working directory to client fd
 *
 * @param state
 * @param argc
 * @return int
 */
int pwd(session_state_t *state, int argc) {
    if (argc != 0) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    if (strcmp(state->cwd, root_directory) == 0) {
        dprintf(state->clientfd, "257 \"/\"\r\n");
    } else {
        dprintf(state->clientfd, "257 \"%s\"\r\n", &state->cwd[strlen(root_directory)]);
    }
    return 0;
}

/**
 * Changes CWD in user state to the given relative path arg[0]
 *
 * @param state
 * @param argc
 * @param args
 * @return 0
 */
int cwd(session_state_t *state, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    char dirpath[PATH_LEN];
    if (to_absolute_path(args[0], state->cwd, dirpath) == 0) {
        dprintf(state->clientfd, "550 Failed to change directory.\r\n");
        return 0;
    }
    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        dprintf(state->clientfd, "550 No such directory.\r\n");
        return 0;
    }
    closedir(dir);
    strcpy(state->cwd, dirpath);
    printf("CWD %s 250\r\n", state->cwd);
    dprintf(state->clientfd, "250 Working directory changed.\r\n");
    return 0;
}

/**
 * Sets CWD for client session to parent directory of CWD if CWD is not root
 *
 * @param state
 * @param argc
 * @return 0
 */
int cdup(session_state_t *state, int argc) {
    if (argc != 0) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }
    if (strcmp(state->cwd, root_directory) == 0) {
        dprintf(state->clientfd, "550 Directory not accessible.\r\n");
        return 0;
    }

    // Set last occurrance of '/' to '\0'
    char *end = state->cwd + strlen(state->cwd) - 1;
    while ((unsigned char)*end != '/') end--;
    *end = '\0';

    printf("CDUP 250, CWD=%s\r\n", state->cwd);
    dprintf(state->clientfd, "250 Working directory changed.\r\n");
    return 0;
}

/**
 * Sends status 200 if type arg is a valid type; else sends error status code
 *
 * @param state
 * @param argc
 * @param args
 * @return 0
 */
int set_type(session_state_t *state, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    char *type = args[0];
    if (strcasecmp(type, "A") == 0) {
        dprintf(state->clientfd, "200 Set to ASCII type.\r\n");
    } else if (strcasecmp(type, "I") == 0) {
        dprintf(state->clientfd, "200 Set to Image type.\r\n");
    } else {
        dprintf(state->clientfd, "504 Unsupported type-code. Only type A and I are allowed.\r\n");
    }
    return 0;
}

/**
 * Sends status 200 if type arg is a valid mode; else sends error status code
 *
 * @param state
 * @param argc
 * @param args
 * @return 0
 */
int set_mode(session_state_t *state, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    char *mode = args[0];
    if (strcasecmp(mode, "S") == 0) {
        dprintf(state->clientfd, "200 Set to streaming mode.\r\n");
    } else {
        dprintf(state->clientfd, "504 Unsupported mode-code. Only type S is allowed.\r\n");
    }
    return 0;
}

/**
 * Sends status 200 if file_structure arg is a valid file structure;
 * else sends error code
 *
 * @param state
 * @param argc
 * @param args
 * @return 0
 */
int set_file_structure(session_state_t *state, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    char *file_structure = args[0];
    if (strcasecmp(file_structure, "F") == 0) {
        dprintf(state->clientfd, "200 Set to file structure.\r\n");
    } else {
        dprintf(state->clientfd,
                "504 Unsupported structure-code. Only type F is allowed.\r\n");
    }
    return 0;
}

/**
 * Writes bytes from user input filename to DTP client fd if file exists
 * https://idiotdeveloper.com/file-transfer-using-tcp-socket-in-c/
 *
 * @param state
 * @param argc
 * @param args
 * @return 0
 */
int retrieve_file(session_state_t *state, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    connection_t *connection = &state->data_connection;
    if (!connection->awaiting_client && connection->clientfd == -1) {
        dprintf(state->clientfd, "425 Use PASV first.\r\n");
        return 0;
    }
    if (connection->awaiting_client) {
        dprintf(state->clientfd,
                "425 Data connection has not been established.\r\n");
        return 0;
    }

    char filepath[PATH_LEN];
    if (to_absolute_path(args[0], state->cwd, filepath) == 0) {
        dprintf(state->clientfd, "550 File path not allowed.\r\n");
        return 0;
    }
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        dprintf(state->clientfd, "550 File does not exist.\r\n");
        return 0;
    }

    dprintf(state->clientfd, "150 Opening data connection for %s.\r\n", filepath);
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
                dprintf(state->clientfd, "550 Could not send file.\r\n");
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
    dprintf(state->clientfd, "226 Transfer complete.\r\n");
    close_connection(connection);
    return 0;
}

/**
 * Initializes new DTP socket to listen for clients in session state
 *
 * @param state
 * @param argc
 * @return 0
 */
int passive_mode(session_state_t *state, int argc) {
    if (argc != 0) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    connection_t *connection = &state->data_connection;
    if (connection->awaiting_client || connection->clientfd != -1) {
        close_connection(connection);
    }
    open_passive_port(state);
    int port = get_socket_port(connection->passivefd);
    dprintf(state->clientfd,
            "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
            hostip_octets[0], hostip_octets[1], hostip_octets[2],
            hostip_octets[3], port >> 8, port & 0xff);
    return 0;
}

/**
 * Lists directory entries in CWD to DTP client fd
 *
 * @param state
 * @param argc
 * @return 0
 */
int nlst(session_state_t *state, int argc) {
    if (argc != 0) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
        return 0;
    }

    connection_t *connection = &state->data_connection;
    if (!connection->awaiting_client && connection->clientfd == -1) {
        dprintf(state->clientfd, "425 Use PASV first.\r\n");
        return 0;
    }
    if (connection->awaiting_client) {
        dprintf(state->clientfd, "425 Data connection has not been established.\r\n");
        return 0;
    }
    dprintf(state->clientfd, "150 Here comes the directory listing.\r\n");
    listFiles(connection->clientfd, state->cwd);
    dprintf(state->clientfd, "226 Directory send OK.\r\n");
    close_connection(connection);
    return 0;
}

/**
 * Binds socket to data port
 * 
 * @param state 
 * @param argc 
 * @param args 
 * @return 0
 */
int data_port(session_state_t *state, int argc, char *args[]) {
    if (argc != 1) {
        dprintf(state->clientfd, "501 Incorrect number of parameters.\r\n");
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
        dprintf(state->clientfd, "500 Illegal PORT command.\r\n");
        return 0;
    }
    char ipaddr[256];
    strcpy(ipaddr, tokens[0]);
    for (int i = 1; i < 4; i++) {
        strcat(ipaddr, ".");
        strcat(ipaddr, tokens[i]);
    }
    printf("%s\n", ipaddr);
    int port = (atoi(tokens[4]) << 8) + atoi(tokens[5]);
    printf("%d\n", port);
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

    state->data_connection.passivefd = passivefd;
    state->data_connection.clientfd = clientfd;
    state->data_connection.awaiting_client = 0;
    return 0;
}

/**
 * Exposes a new port for a DTP connection for a given session state
 *
 * @param state session state
 * @return 1 on successful DTP socket bind; else 0
 */
int open_passive_port(session_state_t *state) {
    connection_t *connection = &state->data_connection;
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
        (void *)state
    );
    return 1;
}

/**
 * Awaits DTP client to accept or times out if client does not connect
 * within DTP_TIMEOUT_SECONDS
 *
 * @param state_data bytes representing client control session state
 * @return NULL
 */
void *accept_data_client(void *state_data) {
    session_state_t *state = state_data;
    connection_t *connection = &state->data_connection;

    // https://linux.die.net/man/2/select
    struct timeval tv;
    fd_set readfds;
    tv.tv_sec = DTP_TIMEOUT_SECONDS;
    tv.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(connection->passivefd, &readfds);
    if (select(connection->passivefd + 1, &readfds, NULL, NULL, &tv) == 0) {
        dprintf(state->clientfd, "421 Timeout.\r\n");
        close_connection(connection);
        return NULL;
    }

    struct sockaddr_in sin;
    socklen_t addrlen = sizeof(sin);
    connection->clientfd = accept(connection->passivefd, (struct sockaddr *) &sin, &addrlen);
    if (connection->clientfd < 0) {
        dprintf(state->clientfd, "425 Could not open data connection.\r\n");
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
    pthread_cancel(connection->accept_client_t);
    pthread_join(connection->accept_client_t, NULL);
    connection->awaiting_client = 0;
    close(connection->passivefd);
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
    while (istrimmable(*str)) str++;
    if (*str == 0) {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && istrimmable(*end)) end--;
    end[1] = '\0';
    return str;
}

int istrimmable(unsigned char chr) {
    return isspace(chr) || chr == '\r' || chr == '\n';
}
