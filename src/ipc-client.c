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
#include <errno.h>
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
#define OUT_CMD		"output "
#define OUT_LEN		7

extern int sock_fd;
static char *sock_path;
size_t device_len;
char device_cmd[MAX_SHORT_RESP];

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

/**
 * Part of this function resembles ipc_read(), but because
 * the length of the output of GET_OUTPUTS may vary significantly
 * across devices, this type of reponse is processed on the heap.
 *
 * This function currently obtains an identifier of the first output
 * among those returned by SwayWM - it is assumed to be the primary one.
 */
static bool set_device() {
	uint32_t len;
	char *buf = malloc(HDR_LEN);
	if (!buf) {
		g_printerr("Allocation failed for display identifier: %s\n",
				strerror(errno));
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

	const char field[] = "\"name\": \"";
	char *start = strstr(buf + HDR_LEN, field);
	if (!start) {
		g_printerr("Unexpected GET_OUTPUTS response.\n");
		goto error;
	}

	start += sizeof(field) - 1; /* exclude '\0' */
	char *end = strchr(start, '\"');

	/* Open question: maybe cast strlen() to a char, given that
	 * MAX_SHORT_RESP already intdroduces size constraint? */
	device_len = end ? (size_t)(end - start) : strlen(start);
	memcpy(device_cmd, OUT_CMD, OUT_LEN);
	memcpy(device_cmd + OUT_LEN, start, device_len);
	device_len += OUT_LEN;
	device_cmd[device_len] = '\0';

	free(buf);
	return true;

error:
	if (buf)
		free(buf);
	return false;
}

/**
 * This function asks ipc_parse() to parse header, deduces length,
 * and then asks the same function to read payload.
 * Checks whether WM reported success.
 * */
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

/**
 * Messages, with the maximum size of HDR_LEN + MAX_PAYLOAD = 128 bytes,
 * are supposed to be short and are written to socket from the stack.
 * Therefore, the while loop was added just in case something prevents
 * write() to deliver a message even so short.
 *
 * SwayWM IPC socket already accepts commands and returns responses
 * in _native byte order_, thus the only compatibility addition here
 * is to use unsigned integers with the length of 4 bytes.
 */
bool ipc_send(message_t type, const char *payload) {
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
	while (sent < payload_len){
		n = write(sock_fd, message + sent, payload_len - sent);
		if (n < 0) {
			perror("ipc write");
			return false;
		}
		sent += n;
	}

	return ipc_read(type);
}

/**
 * This function is only called at the initial (setup) stage.
 */
bool ipc_connect() {
	sock_path = getenv("SWAYSOCK");
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
