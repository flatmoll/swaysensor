#pragma once

/**
 * Callback for GDBus signal subscription.
 * Unwraps the variant and matches device,
 * then calls the corresponding function.
 * */
void sensor_handler(
	struct _GDBusConnection *conn,
	const char *sender,
	const char *path,
	const char *interface,
	const char *signal,
	struct _GVariant *params,
	void *
);
