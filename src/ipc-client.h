#pragma once

#define MAX_PAYLOAD	178
#define MAX_SHORT_RESP	96
#define MAX_HYPR_PATH	100

/**
 * Select IPC message types.
 * FIXME Expand to account for Hyprland.
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
 * Send one of IPC message types.
 * @param type message type
 * @param payload payload
 * @return false on error
 */
bool ipc_send(message_t type, const char *payload);
