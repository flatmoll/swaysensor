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

#define IPC_MAGIC	"i3-ipc"
#define IPC_LEN		6
#define HDR_LEN		14

extern int sock_fd;
static char *sock_path;
unsigned int wm_spec;
unsigned int wm_accel;

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
	const char wm_dev_field[2][10] = {
		"\"name\": \"",
		"Monitor",
	};
	unsigned const char wm_dev_char[] = { 34, 32, };

	uint32_t len;
	char *buf = malloc(HDR_LEN);
	if (!buf) {
		perror("set device malloc");
		goto error;
	}

	if (!ipc_parse(buf, HDR_LEN)) {
		perror("parse device header");
		goto error;
	}

	memcpy(&len, buf + IPC_LEN, 4);	
	if (len > 0) {
		char *tmp = realloc(buf, HDR_LEN + len + 1);
		if (!tmp) {
			perror("set device realloc");
			goto error;
		}
		buf = tmp;
	}

	if (!ipc_parse(buf + HDR_LEN, len)) {
		perror("parse device payload");
		goto error;
	}

	bool wm_dev_id = (wm_spec == 4);
	char *start = strstr(buf + HDR_LEN, wm_dev_field[wm_dev_id]);
	if (!start) {
		g_printerr("Unexpected output information format.\n");
		goto error;
	}

	start += strlen(wm_dev_field[wm_dev_id]);
	char *end = strchr(start, wm_dev_char[wm_dev_id]);

	device_len = end ? (size_t)(end - start) : strlen(start);
	memcpy(device_cmd, start, device_len);
	device_cmd[device_len] = '\0';

	free(buf);
	return true;

error:
	if (buf)
		free(buf);
	return false;
}

static bool ipc_read(message_t type) {
	if (type == 3)
		return set_device();

	uint32_t len;	
	char buf[MAX_SHORT_RESP];

	if (!ipc_parse(buf, HDR_LEN)) {
		perror("parse header");
		return false;
	}

	memcpy(&len, buf + IPC_LEN, 4);

	if (!ipc_parse(buf + HDR_LEN, len)) {
		perror("parse payload");
		return false;
	}

	return strstr(buf + HDR_LEN, "\"success\": true") != NULL;
}

/* AFAIK native byte order is supported by most window managers. */
bool ipc_send(message_t type, const char *payload) {
	static GMutex mutex;

	type = (uint32_t)type;	
	uint32_t payload_len = (uint32_t)strlen(payload);
	uint8_t message[HDR_LEN + payload_len];	
	size_t sent = 0;
	ssize_t n;

	if (payload_len > MAX_PAYLOAD) {
		g_printerr("Payload size limit exceeded.\n");
		return false;
	}

	memcpy(message, IPC_MAGIC, IPC_LEN);
	memcpy(message + IPC_LEN, &payload_len, 4);
	memcpy(message + IPC_LEN + 4, &type, 4);
	memcpy(message + IPC_LEN + 8, payload, payload_len);	
	payload_len += HDR_LEN;

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
		wm_spec = 0;
		wm_accel = 0;
	} else if ((env = getenv("I3SOCK"))) {
		wm_spec = 0;
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
		printf("%s\n", hypr_env);
		
		wm_spec = 4;
		wm_accel = 11;
		env = hypr_env;
	} else {
		g_printerr("Unmatched WM environment.\n");
		return NULL;
	}
	/* FIXME Hyprland
	 * Define custom commands. Move literals from ipc_read()
	 * and set_device() into a static array controlled by this
	 * function. Move handler commands into a global array. */

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
