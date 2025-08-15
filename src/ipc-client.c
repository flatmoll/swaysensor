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
#include <glib.h>
#include <gio/gio.h>

#include "ipc-client.h"

#define SWAY_I3X_MAGIC	"i3-ipc"
#define SWAY_I3X_IPCLEN 6
#define SWAY_I3X_HDRLEN 14
#define HYPR_LONG_RESP	2048

int sock_fd = -1;
static char *sock_path;

wmenv_t wm_spec;
wmaccel_t wm_accel;

size_t device_len;
char device_cmd[MAX_SHORT_RESP];
char synth_sock[MAX_SOCK_PATH];

/* Branch for i3 is commented out and scheduled for another time.
 * It turns out that I3SOCK may not necessarily be set, thus the
 * socket path must be retrieved as an X root window property,
 * which will be implemented in get_xprop(), probably in a separate
 * utils file. For now, Wayland compositors are prioritized. */
static char *determine_environment() {
	char *env = NULL;

	if ((env = getenv("SWAYSOCK"))) {
		wm_spec = SWAY_I3X;
		wm_accel = SWAY_I3X_ACCEL;
	/*
	} else if ((env = getenv("I3SOCK")) || (env = get_xprop(SWAY_I3X))){
		wm_spec = SWAY_I3X;
	 	wm_accel = SWAY_I3X_ACCEL;
	*/
	} else if ((env = getenv("HYPRLAND_INSTANCE_SIGNATURE"))) {
		char *dir = getenv("XDG_RUNTIME_DIR");
		if (!dir)
			return NULL;

		if (snprintf(synth_sock,
			MAX_SOCK_PATH,
			"%s/hypr/%s/.socket.sock",
			dir,
			env) == 0)
		{
			g_printerr("Failed to set Hyprland environment.\n");
			return NULL;
		}
		
		wm_spec = HYPRLAND;
		wm_accel = HYPRLAND_ACCEL;
		env = synth_sock;
	} else {
		g_printerr("Unmatched WM environment.\n");
		return NULL;
	}

	return env;
}

bool ipc_connect() {
	if (!sock_path) {
		sock_path = determine_environment();		
		if (!sock_path) {
			g_printerr("Could not find socket path.\n");
			return false;
		}
		if (wm_spec == HYPRLAND)
			return true;
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

static inline bool ipc_write(unsigned char *buf, size_t len) {
	static GMutex m_write;
	size_t sent = 0;
	ssize_t n;

	g_mutex_lock(&m_write);
	while (sent < len) {
		n = write(sock_fd, buf + sent, len - sent);
		if (n == 0) {
			g_mutex_unlock(&m_write);
			return true;
		} else if (n < 0) {
			g_mutex_unlock(&m_write);
			return false;
		}
		sent += n;
	}

	g_mutex_unlock(&m_write);
	return true;
}

static inline bool ipc_parse(char *buf, size_t len) {
	static GMutex m_parse;
	size_t received = 0;
	ssize_t n;
	
	g_mutex_lock(&m_parse);
	while (received < len) {
		n = recv(sock_fd, buf + received, len - received, 0);
		if (n == 0) {
			g_mutex_unlock(&m_parse);
			return true;
		} else if (n < 0) {
			g_mutex_unlock(&m_parse);
			return false;			
		}
		received += n;
	}
	
	g_mutex_unlock(&m_parse);
	return true;
}

static inline bool hypr_set_device() {
	uint32_t len = HYPR_LONG_RESP;
	char *buf = malloc(HYPR_LONG_RESP);
	if (!buf) {
		perror("[Hyprland] set device malloc");
		goto error;
	}

	if (!ipc_parse(buf, len)) {
		perror("[Hyprland] parse device payload");
		goto error;
	}

	char field[] = "Monitor ";
	char *start = strstr(buf, field);
	if (!start) {
		g_printerr("Unexpected output information format.\n");
		goto error;
	}

	start += sizeof(field) - 1;
	char *end = strchr(start, 32);

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


static inline bool hypr_read_status() {
	char buf[MAX_SHORT_RESP];

	if (!ipc_parse(buf, sizeof(buf))) {
		perror("[Hyprland] parse payload");
		return false;
	}

	return strstr(buf, "ok") != NULL;
}

static inline bool hypr_interact(swaymsg_t type, const char *payload) {
	if (!ipc_connect()) {
		g_printerr("Reconnect to Hyprland failed.\n");
		return false;
	}

	char dev[] = "monitors";
	const char *new_payload = (type == RUN_COMMAND) ? payload : dev;
	uint32_t payload_len = (uint32_t)strlen(new_payload);	

	if (!ipc_write((unsigned char *)new_payload, payload_len)) {
		perror("[Hyprland] send payload");
		return false;
	}

	if (type == GET_OUTPUTS)
		return hypr_set_device();

	return hypr_read_status();
}

/* Currently an identifier of the first listed output is obtained.
 * Memory is allocated because output length varies significantly. */
static inline bool sway_set_device() {
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

	char field[] = "\"name\": \"";
	char *start = strstr(buf + SWAY_I3X_HDRLEN, field);
	if (!start) {
		g_printerr("Unexpected output information format.\n");
		goto error;
	}

	start += sizeof(field) - 1;
	char *end = strchr(start, '\"');

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

static inline bool sway_read_status() {
	uint32_t resp_len;
	char buf[MAX_SHORT_RESP];

	if (!ipc_parse(buf, SWAY_I3X_HDRLEN)) {
		perror("[Sway/i3] parse header");
		return false;
	}

	memcpy(&resp_len, buf + SWAY_I3X_IPCLEN, sizeof(resp_len));

	if (!ipc_parse(buf + SWAY_I3X_HDRLEN, resp_len)) {
		perror("[Sway/i3] parse payload");
		return false;
	}

	return strstr(buf + SWAY_I3X_HDRLEN, "\"success\": true") != NULL;
}

static inline bool sway_interact(swaymsg_t type, const char *payload) {
	uint32_t payload_len = (uint32_t)strlen(payload);	
	uint8_t message[SWAY_I3X_HDRLEN + payload_len];
	type = (uint32_t)type;

	memcpy(message, SWAY_I3X_MAGIC, SWAY_I3X_IPCLEN);
	memcpy(message + SWAY_I3X_IPCLEN, &payload_len, 4);
	memcpy(message + SWAY_I3X_IPCLEN + 4, &type, 4);
	memcpy(message + SWAY_I3X_HDRLEN, payload, payload_len);
	payload_len += SWAY_I3X_HDRLEN;
	
	if (!ipc_write(message, payload_len)) {
		perror("[Sway/i3] send payload\n");
		return false;
	}

	if (type == GET_OUTPUTS)
		return sway_set_device();

	return sway_read_status();
}

bool ipc_send(swaymsg_t type, const char *payload) {
	switch (wm_spec) {
	case SWAY_I3X:
		return sway_interact(type, payload);
	case HYPRLAND:
		return hypr_interact(type, payload);
	default:
		g_assert_not_reached();
	}

	return false;
}
