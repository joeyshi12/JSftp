#ifndef __FTPSERVICE_H__
#define __FTPSERVICE_H__

#include <pthread.h>

#include "tcpserver.h"

#define USER "anonymous"
#define DTP_TIMEOUT_SECONDS 60
#define PATH_LEN 1024
#define MAX_NUM_ARGS 4
#define NUM_CMDS 15

typedef struct connection_s {
    int passivefd;
    int clientfd;
    int awaiting_client;
    pthread_t accept_client_t;  // cancel old awaiting connections
} connection_t;

typedef enum {
    STATE_OPEN,
    STATE_AWAITING_USER,
    STATE_AWAITING_PASS,
    STATE_ACTIVE,
    STATE_EXITED
} session_state_t;

typedef struct client_session_s {
    int clientfd;
    char cwd[PATH_LEN];
    connection_t data_connection;
    session_state_t state;
    pthread_t session_thread;
} client_session_t;

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
    CMD_PORT,
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

int execute_cmd(cmd_t cmd, int argc, char *args[], client_session_t *state);

// Command functions
int handle_user(client_session_t *state, int argc, char *args[]);
int handle_pass(client_session_t *state, int argc);
int handle_pwd(client_session_t *state, int argc);
int handle_cwd(client_session_t *state, int argc, char *args[]);
int handle_cdup(client_session_t *state, int argc);
int handle_type(client_session_t *state, int argc, char *args[]);
int handle_mode(client_session_t *state, int argc, char *args[]);
int handle_stru(client_session_t *state, int argc, char *args[]);
int handle_retr(client_session_t *state, int argc, char *args[]);
int handle_port(client_session_t *state, int argc, char *args[]);
int handle_pasv(client_session_t *state, int argc);
int handle_nlst(client_session_t *state, int argc);

// DTP connection handling
int open_passive_port(client_session_t *state);
void *accept_data_client(void *state);
void close_connection(connection_t *connection);

// Helper functions
cmd_t to_cmd(char *str);
int to_absolute_path(char *relpath, char cwd[], char outpath[]);
char *trimstr(char *str);
int istrimchar(unsigned char chr);

#endif
