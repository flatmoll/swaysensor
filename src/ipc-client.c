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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "ipc-client.h"

#define IPC_MAGIC	"i3-ipc"
#define IPC_LEN		6
#define HDR_LEN		14
#define MAX_PAYLOAD	242
#define MAX_SHORT_RESP	128

static char *sock_path;
static char device[MAX_SHORT_RESP];

/**
 * This function currently obtains an identifier of the first output
 * among those returned by SwayWM - it is assumed to be the primary one.
 */
static void set_device(const char *buf) {
	const char field[] = "\"name\": \"";
	char *start = strstr(buf, field);
	if (!start) {
		fprintf(stderr, "Unexpected GET_OUTPUTS response.\n");
		return;
	}

	start += sizeof(field) - 1; /* exclude '\0' */
	char *end = strchr(start, '\"');
	size_t len = end ? (size_t)(end - start) : strlen(start);

	memcpy(device, start, len);
	device[len] = '\0';
	printf("Device known.\n");
}

/**
 * This function first receives header, deduces response length, and
 * then reads this response in the same manner.
 *
 * Because the length of the output of GET_OUTPUTS may vary significantly
 * across devices, this type of reponse is processed on the heap.
 */
static bool ipc_read(message_t type) {
	uint32_t resp_len;
	size_t received = 0;
	ssize_t n;
	char *buf;
	char short_resp[MAX_SHORT_RESP];

	switch (type)
	{
	case 0:
		buf = short_resp;
		break;
	case 3:
		buf = malloc(HDR_LEN);
		if (!buf) {
			perror("ipc malloc");
			return false;
		}
		break;
	default:
		fprintf(stderr, "Unmatched response type: %d.\n", type);
		return false;
	}

	while (received < HDR_LEN) {
		n = recv(sock_fd, buf + received,
				HDR_LEN - received, 0);
		if (n < 0) {
			perror("ipc recv header");
			goto error;
		}
		received += n;
	}

	memcpy(&resp_len, buf + IPC_LEN, 4);
	if (type == 3 && resp_len > 0) {
		char *tmp = realloc(buf, HDR_LEN + resp_len);
		if (!tmp) {
			perror("ipc realloc");
			goto error;
		}
		buf = tmp;
	}

	received = 0;
	while (received < resp_len) {
		n = recv(sock_fd, buf + HDR_LEN + received,
				resp_len - received, 0);
		if (n < 0) {
			perror("ipc recv payload");
			goto error;
		}
		received += n;		
	}
	
	switch (type)
	{
	case 0:
		return strstr(buf + HDR_LEN, "\"success\": true");
	case 3:
		set_device(buf + HDR_LEN);
		free(buf);
		break;
	default:
		return false;
	}

	return device[0] != '\0';

error:
	if (type == 3)
		free(buf);
	return false;
}

/**
 * Messages, with the maximum size of HDR_LEN + MAX_PAYLOAD = 256 bytes,
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
	uint32_t payload_len = payload
				? (uint32_t)strlen(payload) : 0;
	uint8_t message[HDR_LEN + payload_len];	
	size_t sent = 0;
	ssize_t n;

	if (payload_len > MAX_PAYLOAD) {
		fprintf(stderr, "Payload size limit exceeded.\n");
		return false;
	}

	memcpy(message, IPC_MAGIC, IPC_LEN);
	memcpy(message + IPC_LEN, &payload_len, 4);
	memcpy(message + IPC_LEN + 4, &type, 4);
	memcpy(message + IPC_LEN + 8, payload, payload_len);	
	
	while (sent < HDR_LEN + payload_len){
		n = write(sock_fd, message + sent,
				HDR_LEN + payload_len - sent);
		if (n < 0) {
			perror("ipc write");
			return false;
		}
		sent += n;
	}

	return ipc_read(type);
}

/**
 * This is formally async-safe but, due to the nature of this
 * function, only called at the beginning.
 */
bool ipc_connect() {
	sock_path = getenv("SWAYSOCK");
	if (!sock_path) {
		fprintf(stderr, "Could not find socket path.\n");
		return false;
	}

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd == -1) {
		perror("ipc socket");
		return false;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

	if (connect(sock_fd, (struct sockaddr*)&addr,
				sizeof(addr)) == -1) {
		perror("ipc connection");
		close(sock_fd);
		sock_fd = -1;
		return false;
	}

	return true;
}
