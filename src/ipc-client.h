#pragma once
#include <stdbool.h>

#define MAX_PAYLOAD	178
#define MAX_SHORT_RESP	96
#define MAX_HYPR_PATH	100

/**
 * Select IPC message types and environments.
 */
typedef enum {
	RUN_COMMAND = 0,
	GET_OUTPUTS = 3,
} swaymsg_t;

typedef enum {
	SWAY_I3X = 0,
	HYPRLAND = 4,
} wmenv_t;

typedef enum {
	SWAY_I3X_ACCEL = 0,
	HYPRLAND_ACCEL = 11,
} wmaccel_t;

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
bool ipc_send(swaymsg_t type, const char *payload);
