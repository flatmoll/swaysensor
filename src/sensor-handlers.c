#include <stdio.h>
#include <gio/gio.h>

#include "sensor-handlers.h"

void accel(
	struct _GDBusConnection *conn,
	const char *sender,
	const char *path,
	const char *interface,
	const char *signal,
	struct _GVariant *params,
	void *
) {
	printf("Caught accel.\n");
}

void light(
	struct _GDBusConnection *conn,
	const char *sender,
	const char *path,
	const char *interface,
	const char *signal,
	struct _GVariant *params,
	void *
) {
	printf("Caught light.\n");
}

void proximity(
	struct _GDBusConnection *conn,
	const char *sender,
	const char *path,
	const char *interface,
	const char *signal,
	struct _GVariant *params,
	void *
) {
	printf("Caught proximity.\n");
}

