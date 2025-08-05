#pragma once

#define MAX_PAYLOAD	114
#define MAX_SHORT_RESP	96

/**
 * Select Sway IPC message types.
 */
typedef enum {
	RUN_COMMAND = 0,
	GET_OUTPUTS = 3,
} message_t;

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
