/**
 * SPDX-License-Identifier: MIT
 * swaysensor: integration of iio-sensor-proxy into SwayWM.
 * Copyright (C) 2025 Fuad Veliev <fuad@grrlz.net>
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <gio/gio.h>
#include <glib-unix.h>

#include "ipc-client.h"
#include "gdbus-client.h"

/**
 * Device polling user options.
 * If true, a polling request will be sent to the proxy.
 * An entry may be set back to false by gdbus_connect()
 * if the corresponding device is unavailable. This prevents
 * the release of unclaimed devices during a termination.
 */
bool devices[NUM_DEV] = {
	false, /* accelerometer */
	false, /* light level */
	false, /* proximity */
};

static int fd;
int sock_fd = -1;
static GMainLoop *loop;

static gboolean g_handle(gpointer data) {
	printf(" - Received signal, exiting.\n");
	g_main_loop_quit(loop);
	return G_SOURCE_REMOVE;
}

/**
 * Such a big and varied main function provides a concise visual
 * list of nearly everything (on a high level) that has been done
 * before the program entered a listening state.
 *
 * The program uses a lock file in $XDG_RUNTIME_DIR to prevent
 * accidental multiple launches.
 */
int main(int argc, char **argv) {
	if (argc == 1) {
		printf("Usage: %s [OPTIONS]\n"
			"    ""-a  enable accelerometer\n"
			"    ""-l  enable ambient light sensor\n"
			"    ""-p  enable proximity sensor\n",
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

	/* Len ("/run/user" + max UID + "/swaysensor.pid" + '\0') = 36 */
	char pidfile[36];
	/* Len (max PID + "\0"), with ceiling to nearest power of 2. */
	char pid_buf[16];

	c = snprintf(pidfile, sizeof(pidfile), "%s%s",
				getenv("XDG_RUNTIME_DIR"),
				"/swaysensor.pid");
	if (!c) {
		fprintf(stderr, "Could not get runtime directory.\n");
		return EXIT_FAILURE;
	}

	fd = open(pidfile, O_RDWR | O_CREAT, 0644);
	if (fd == -1) {
		perror("open pidfile");
		return EXIT_FAILURE;
	}

	if (flock(fd, LOCK_EX|LOCK_NB) == -1) {
		if (errno == EWOULDBLOCK)
			fprintf(stderr, "Another instance is running.\n");
		else
			perror("flock");
		close(fd);
		return EXIT_FAILURE;
	}

	if (ftruncate(fd, 0) == -1)
		perror("file truncation");
	
	c = snprintf(pid_buf, sizeof(pid_buf),
				"%ld\n", (long)getpid());
	if (write(fd, pid_buf, c) == -1)
		perror("write pid");

	if (!ipc_connect()) {
		fprintf(stderr, "Could not connect to Sway socket.\n");
		close(fd);
		return EXIT_FAILURE;
	}

	/* Due to brightnessctl, display identifier is only used
	 * with accelerometer and proximity sensor. */
	if (devices[0] || devices[2]) {
		if (!ipc_send(GET_OUTPUTS, "")) {
			fprintf(stderr,
				"Could not obtain display identifier.\n");
			close(sock_fd);
			close(fd);
			return EXIT_FAILURE;
		}
	}

	if (!gdbus_connect()) {
		fprintf(stderr, "Could not connect to sensor proxy.\n");
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
