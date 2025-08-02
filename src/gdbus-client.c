/**
 * SPDX-License-Identifier: MIT
 * GNOME Desktop Bus Client: communication with proxy via D-Bus protocol.
 * Copyright (C) 2025 Fuad Veliev <fuad@grrlz.net>
 *
 * gdbus_connect() and gdbus_close(), perform step-by-step connection
 * and termination of such, respectively. Neither of them is anync-safe.
 * Instead, the structure of main (main.c) guarantees that neither
 * of them is called while there exists an active GMainLoop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <gio/gio.h>

#include "gdbus-client.h"
#include "sensor-handlers.h"

#define PROXY_NAME "net.hadess.SensorProxy"
#define PROXY_PATH "/net/hadess/SensorProxy"
#define PROPS_NAME "org.freedesktop.DBus.Properties"
#define PROPS_TYPE "PropertiesChanged"

/**
 * Pointers to commands to Claim and Release proxy methods,
 * as well as handler functions. property_connections stores
 * identifiers for connections that listen to property changes,
 * in order to release these connections upon cleaning up.
 *
 * is_light_vendor indicates whether the brightness unit is
 * vendor-defined (true, default) or SI lux (false).
 */
static const char * const claim_cmd[NUM_DEV] = {
	"ClaimAccelerometer",
	"ClaimLight",
	"ClaimProximity",
};

static const char * const release_cmd[NUM_DEV] = {
	"ReleaseAccelerometer",
	"ReleaseLight",
	"ReleaseProximity",
};

static guint property_connections[NUM_DEV];

void (*sensor_handlers[NUM_DEV])(
	struct _GDBusConnection *,
	const char *,
	const char *,
	const char *,
	const char *,
	struct _GVariant *,
	void *
) = {
	accel,
	light,
	proximity,
};

static GDBusConnection *connection;
static GDBusProxy *proxy;
static GVariant *result;
static GError *error;

bool is_light_vendor = true;

/**
 * Closes connection to GDBus or cleans up an unsuccessful one,
 * in the order that is reverse to one of gdbus_connect().
 *
 * If releasing a device yields an unexpected error,
 * print it but do not return failure, since this function
 * is to be run shortly before program exits and should
 * clean up as much as possible.
 */
void gdbus_close() {
	for (short i = 0; i < NUM_DEV; i++) {
		if (devices[i]) {
			g_dbus_connection_signal_unsubscribe(
				connection,
				property_connections[i]
			);
			result = g_dbus_proxy_call_sync(
					proxy,
					release_cmd[i],
					NULL,
					G_DBUS_PROXY_FLAGS_NONE,
					-1,
					NULL,
					&error
			);
			if (!result) {
				g_printerr(
					"Could not release device: %s\n",
					error->message
				);
				g_clear_error(&error);
			}
		}
	}

	g_clear_error(&error);
	error = NULL;

	g_variant_unref(result);
	result = NULL;

	g_object_unref(proxy);
	proxy = NULL;

	g_object_unref(connection);
	connection = NULL;
}

/**
 * Performs a connection to net.hadess.SensorProxy via GDBus.
 * Returns false on general connection failures or if no
 * devices were registered.
 *
 * If a device could not be registered, either because it is not present
 * or because of a bug in this program, print the error, but do not
 * fail, unless there are no devices registered at all.
 */
bool gdbus_connect() {
	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!connection) {
		g_printerr("gdbus connect: %s\n", error->message);
		g_clear_error(&error);
		return false;
	}

	proxy = g_dbus_proxy_new_sync(
			connection,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			PROXY_NAME,
			PROXY_PATH,
			PROXY_NAME,			
			NULL, 
			&error
	);
	if (!proxy) {
		g_printerr("gdbus proxy: %s\n", error->message);
		g_clear_error(&error);
		return false;
	}
	
	short reg = 0;
	for (short i = 0; i < NUM_DEV; i++) {
		if (devices[i]) {
			result = g_dbus_proxy_call_sync(
					proxy,
					claim_cmd[i],
					NULL,
					G_DBUS_PROXY_FLAGS_NONE,
					-1,
					NULL,
					&error
			);
			if (result) {
				property_connections[i] =
				g_dbus_connection_signal_subscribe(
					connection,
					PROXY_NAME,
					PROPS_NAME,
					PROPS_TYPE,
					PROXY_PATH,
					NULL,
					G_DBUS_SIGNAL_FLAGS_NONE,
					sensor_handlers[i],
					NULL,
					NULL
				);
				reg++;
				printf("Some registration done.\n");
			} else {
				g_printerr(
					"Could not claim device: %s\n",
					error->message
				);
				g_clear_error(&error);
				devices[i] = false;
			}
		}
	}

	if (reg == 0) {
		fprintf(stderr, "No devices were registered.\n");
		return false;
	}

	return true;
}
