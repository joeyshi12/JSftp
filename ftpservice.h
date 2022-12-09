#ifndef __FTPSERVICE_H__
#define __FTPSERVICE_H__

#include <pthread.h>

#include "tcpserver.h"

#define USER "cs317"
#define DTP_TIMEOUT_SECONDS 30
#define PATH_LEN 1024
#define MAX_NUM_ARGS 4
#define NUM_CMDS 14

typedef struct connection_s {
    socket_t server_socket;
    int clientfd;
    int awaiting_client;
    pthread_t accept_client_t;  // cancel old awaiting connections
} connection_t;

typedef struct session_state_s {
    int clientfd;
    connection_t data_connection;
    int is_logged_in;
    char cwd[PATH_LEN];
} session_state_t;

typedef enum {
    CMD_USER,
    CMD_PASS,
    CMD_QUIT,
    CMD_SYST,
    CMD_PWD,
    CMD_CWD,
    CMD_CDUP,
    CMD_TYPE,
    CMD_MODE,
    CMD_STRU,
    CMD_RETR,
    CMD_PASV,
    CMD_LIST,
    CMD_NLST,
    CMD_INVALID
} cmd_t;

typedef struct cmd_map_s {
    char *cmd_str;
    cmd_t cmd;
} cmd_map_t;

extern char root_directory[PATH_LEN];
extern uint8_t hostip_octets[4];
extern cmd_map_t cmd_map[NUM_CMDS];

void *handle_session(void *clientfd);

int execute_cmd(cmd_t cmd, int argc, char *args[], session_state_t *state);

// Command functions
int login_user(session_state_t *state, int argc, char *args[]);
int quit_session(session_state_t *state);
int pwd(session_state_t *state, int argc);
int cwd(session_state_t *state, int argc, char *args[]);
int cdup(session_state_t *state, int argc);
int set_type(session_state_t *state, int argc, char *args[]);
int set_mode(session_state_t *state, int argc, char *args[]);
int set_file_structure(session_state_t *state, int argc, char *args[]);
int retrieve_file(session_state_t *state, int argc, char *args[]);
int passive_mode(session_state_t *state, int argc);
int nlst(session_state_t *state, int argc);
int handle_unknown(session_state_t *state);

// DTP connection handling
int open_passive_port(session_state_t *state);
void *accept_data_client(void *state);
void close_connection(connection_t *connection);

// Helper functions
cmd_t to_cmd(char *str);
char *trimstr(char *str);
int to_absolute_path(char *relpath, char cwd[], char outpath[]);

#endif