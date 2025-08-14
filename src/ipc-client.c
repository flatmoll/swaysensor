/**
 * SPDX-License-Identifier: MIT
 * Inter Process Communication Client: application control via socket.
 * Copyright (C) 2025 Fuad Veliev <fuad@grrlz.net>
 *
 * This minimal implementation suppports the following command types:
 *	RUN_COMMAND - for reacting to sensor readings, and
 *	GET_OUTPUTS - run once to obtain display identifier.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <gio/gio.h>

#include "ipc-client.h"
#include "glib.h"

#define SWAY_I3X_MAGIC	"i3-ipc"
#define SWAY_I3X_IPCLEN 6
#define SWAY_I3X_HDRLEN 14

extern int sock_fd;
static char *sock_path;

wmenv_t wm_spec;
wmaccel_t wm_accel;

size_t device_len;
char device_cmd[MAX_SHORT_RESP];
char hypr_env[MAX_HYPR_PATH];

static bool ipc_parse(char *buf, size_t len) {
	size_t received = 0;
	ssize_t n;
	
	while (received < len) {
		n = recv(sock_fd, buf + received, len - received, 0);
		if (n < 0)
			return false;
		received += n;
	}
	
	return true;
}

/* Currently an identifier of the first listed output is obtained.
 * Memory is allocated because output length varies significantly. */
static bool set_device() {
	const char wm_field[2][10] = { "\"name\": \"", "Monitor", };
	unsigned const char wm_char[2] = { 34, 32, };

	uint32_t len;
	char *buf = malloc(SWAY_I3X_HDRLEN);
	if (!buf) {
		perror("set device malloc");
		goto error;
	}

	if (!ipc_parse(buf, SWAY_I3X_HDRLEN)) {
		perror("parse device header");
		goto error;
	}

	memcpy(&len, buf + SWAY_I3X_IPCLEN, sizeof(len));
	
	if (len > 0) {
		char *tmp = realloc(buf, SWAY_I3X_HDRLEN + len + 1);
		if (!tmp) {
			perror("set device realloc");
			goto error;
		}
		buf = tmp;
	}

	if (!ipc_parse(buf + SWAY_I3X_HDRLEN, len)) {
		perror("parse device payload");
		goto error;
	}

	bool wm_display = (wm_spec == HYPRLAND);
	char *start = strstr(buf + SWAY_I3X_HDRLEN, wm_field[wm_display]);
	if (!start) {
		g_printerr("Unexpected output information format.\n");
		goto error;
	}

	start += strlen(wm_field[wm_display]);
	char *end = strchr(start, wm_char[wm_display]);

	device_len = end ? (size_t)(end - start) : 0;
	memcpy(device_cmd, start, device_len);
	device_cmd[device_len] = '\0';

	free(buf);
	return (device_len != 0);

error:
	if (buf)
		free(buf);
	return false;
}

static bool ipc_read(swaymsg_t type) {
	if (type == GET_OUTPUTS)
		return set_device();

	uint32_t len;	
	char buf[MAX_SHORT_RESP];

	if (!ipc_parse(buf, SWAY_I3X_HDRLEN)) {
		perror("parse header");
		return false;
	}

	memcpy(&len, buf + SWAY_I3X_IPCLEN, sizeof(len));

	if (!ipc_parse(buf + SWAY_I3X_HDRLEN, len)) {
		perror("parse payload");
		return false;
	}

	return strstr(buf + SWAY_I3X_HDRLEN, "\"success\": true") != NULL;
}

// FIXME Message format for Hyprland
bool ipc_send(swaymsg_t type, const char *payload) {
	static GMutex mutex;

	unsigned int header_len = (wm_spec == SWAY_I3X) ? SWAY_I3X_HDRLEN : 0;
	uint32_t payload_len = (uint32_t)strlen(payload);	
	uint8_t message[header_len + payload_len];		

	if (payload_len > MAX_PAYLOAD) {
		g_printerr("Payload size limit exceeded.\n");
		return false;
	}

	if (wm_spec == SWAY_I3X) {
		type = (uint32_t)type;	
		memcpy(message, SWAY_I3X_MAGIC, SWAY_I3X_IPCLEN);
		memcpy(message + SWAY_I3X_IPCLEN, &payload_len, 4);
		memcpy(message + SWAY_I3X_IPCLEN + 4, &type, 4);
		memcpy(message + SWAY_I3X_IPCLEN + 8, payload, payload_len);
		payload_len += SWAY_I3X_HDRLEN;
	} else if (wm_spec == HYPRLAND) {
		char t = '/';
		memcpy(message, &t, 1);
		memcpy(message + 1, payload, payload_len);
	} else {
		g_assert_not_reached();
	}

	size_t sent = 0;
	ssize_t n;
	g_mutex_lock(&mutex);
	while (sent < payload_len){
		n = write(sock_fd, message + sent, payload_len - sent);
		if (n < 0) {
			g_mutex_unlock(&mutex);
			perror("ipc write");
			return false;
		}
		sent += n;
	}
	g_mutex_unlock(&mutex);

	return ipc_read(type);
}

static char *determine_environment() {
	char *env = NULL;

	if ((env = getenv("SWAYSOCK"))) {
		wm_spec = SWAY_I3X;
		wm_accel = 0;
	} else if ((env = getenv("I3SOCK"))) {
		wm_spec = SWAY_I3X;
		wm_accel = 0;
	} else if ((env = getenv("HYPRLAND_INSTANCE_SIGNATURE"))) {
		char *dir = getenv("XDG_RUNTIME_DIR");
		if (!dir)
			return NULL;

		if (snprintf(hypr_env,
			MAX_HYPR_PATH,
			"%s/hypr/%s/.socket.sock",
			dir,
			env) == 0)
		{
			g_printerr("Failed to set Hyprland environment.\n");
			return NULL;
		}
		
		wm_spec = HYPRLAND;
		wm_accel = 11;
		env = hypr_env;
	} else {
		g_printerr("Unmatched WM environment.\n");
		return NULL;
	}

	return env;
}

bool ipc_connect() {
	sock_path = determine_environment();
	if (!sock_path) {
		g_printerr("Could not find socket path.\n");
		return false;
	}

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd == -1) {
		perror("ipc create socket");
		return false;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

	if (connect(sock_fd, (struct sockaddr*)&addr,
				sizeof(addr)) == -1) {
		perror("ipc connect");
		close(sock_fd);
		sock_fd = -1;
		return false;
	}

	return true;
}
