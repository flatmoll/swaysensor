#pragma once

/**
 * Select Sway IPC message types.
 */
typedef enum {
	RUN_COMMAND = 0,
	GET_OUTPUTS = 3,
} message_t;

/**
 * Unlike other IPC variables,
 * socket file descriptor is held by main.
 */
extern int sock_fd;

/**
 * Connect to IPC socket.
 * @return false on error
 */
bool ipc_connect();

/**
 * Send one of Sway IPC message types.
 * @param type message type
 * @param payload payload
 * @return false on error
 */
bool ipc_send(message_t type, const char *payload);
