/**
 * SPDX-License-Identifier: MIT
 * swaysensor: integration of iio-sensor-proxy for window managers.
 * Copyright (C) 2025 Fuad Veliev <fuad@grrlz.net>
 */

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>
#include <gio/gio.h>
#include <glib-unix.h>

#include "ipc-client.h"
#include "gdbus-client.h"

/* Set by user, and if connection fails, reverted by gdbus_connect(). */
bool devices[NUM_DEV] = {
	false, /* accelerometer */
	false, /* light level */
	false, /* proximity */
};

extern int sock_fd;
static GMainLoop *loop;

static gboolean g_handle(gpointer data) {
	g_message("Received signal, exiting.");
	g_main_loop_quit(loop);
	return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
	if (argc == 1) {
		printf("Usage: %s [OPTIONS]\n"
			"  ""-a  poll accelerometer\n"
			"  ""-l  poll ambient light sensor\n"
			"  ""-p  poll proximity sensor\n",
			argv[0]);
		return EXIT_SUCCESS;
	}

	int c;
	while ((c = getopt(argc, argv, "alp")) != -1) {
		switch (c)
		{
		case 'a':
			devices[0] = true;
			break;
		case 'l':
			devices[1] = true;
			break;
		case 'p':
			devices[2] = true;
			break;
		default:
			return EXIT_FAILURE;
		}
	}

	/* Len ("/run/user" + max UID + "/swaysensor.lock" + '\0') = 37 */
	char lockfile[37];
	c = snprintf(lockfile, sizeof(lockfile), "%s/swaysensor.lock",
			getenv("XDG_RUNTIME_DIR"));
	if (c == 0 || c >= sizeof(lockfile)) {
		g_printerr("Could not get runtime directory.\n");
		return EXIT_FAILURE;
	}

	int fd = open(lockfile, O_CREAT | O_WRONLY, 0644);
	if (fd == -1) {
		perror("open pidfile");
		return EXIT_FAILURE;
	}

	if (flock(fd, LOCK_EX|LOCK_NB) == -1) {
		if (errno == EWOULDBLOCK)
			g_printerr("Another instance is running.\n");
		else
			perror("flock");

		close(fd);
		return EXIT_FAILURE;
	}	
	
	if (!ipc_connect()) {
		g_printerr("Could not connect to Sway socket.\n");
		close(fd);
		return EXIT_FAILURE;
	}

	/* Avoid polling IPC for display if all we want is
	 * brightness control through brightnessctl. */
	if (devices[0] || devices[2]) {
		if (!ipc_send(GET_OUTPUTS, "")) {
			g_printerr("Failed to get display identifier.\n");
			close(sock_fd);
			close(fd);
			return EXIT_FAILURE;
		}
	}

	if (!gdbus_connect()) {
		g_printerr("Could not connect to sensor proxy.\n");
		close(sock_fd);
		close(fd);
		return EXIT_FAILURE;
	}

	loop = g_main_loop_new(NULL, FALSE);

	g_unix_signal_add(SIGINT, g_handle, loop);
	g_unix_signal_add(SIGTERM, g_handle, loop);

	g_main_loop_run(loop);	

	gdbus_close();
	close(sock_fd);
	close(fd);
	g_main_loop_unref(loop);
	return EXIT_SUCCESS;
}
