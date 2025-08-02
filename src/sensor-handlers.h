#pragma once

/**
 * Brightness unit, declared in gdbus-client.c.
 */
extern bool is_light_vendor;

/**
 * Handles accelerometer updates: orientation, tilt.
 *
 * Orientation: tells IPC client to ask SwayWM to rotate 
 * the primary display according to the received signal.
 *
 * Tilt: Not defined yet. Please open a feature request
 * and indicate an action that should be performed when
 * the client receives tilt updates.
 */
void accel(
	struct _GDBusConnection *conn,
	const char *sender,
	const char *path,
	const char *interface,
	const char *signal,
	struct _GVariant *params,
	void *
);

/**
 * Handles ambient light sensor updates.
 *
 * TODO Initial calibration (check signal value range).
 */
void light(
	struct _GDBusConnection *conn,
	const char *sender,
	const char *path,
	const char *interface,
	const char *signal,
	struct _GVariant *params,
	void *
);

/**
 * Handles proximity sensor updates.
 *
 * Tells IPC client to ask SwayWM to turn off
 * the primary display when the sensor reports
 * "near" state, and to turn it back on when
 * the signal changes back to normal.
 *
 * This handler does not attempt to invoke
 * suspension or hibernation, and only sends
 * commands of type "sway output".
 */
void proximity(
	struct _GDBusConnection *conn,
	const char *sender,
	const char *path,
	const char *interface,
	const char *signal,
	struct _GVariant *params,
	void *
);
