/**
 * SPDX-License-Identifier: MIT
 * GNOME Desktop Bus Client: communication with proxy via D-Bus protocol.
 * Copyright (C) 2025 Fuad Veliev <fuad@grrlz.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <gio/gio.h>

#include "gdbus-client.h"
#include "sensors.h"

#define PROXY_NAME "net.hadess.SensorProxy"
#define PROXY_PATH "/net/hadess/SensorProxy"
#define PROPS_NAME "org.freedesktop.DBus.Properties"
#define PROPS_TYPE "PropertiesChanged"

extern bool devices[];

/**
 * Pointers to commands to Claim and Release proxy methods.
 * prop_conn stores an identifier for the connection that listen
 * to property changes, in order to release it when cleaning up.
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

static unsigned int prop_conn;

static GDBusConnection *connection;
static GDBusProxy *proxy;
static GVariant *result;
static GError *error;

void gdbus_close() {
	for (int i = 0; i < NUM_DEV; i++) {
		if (devices[i]) {
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

	g_dbus_connection_signal_unsubscribe(connection, prop_conn);

	g_variant_unref(result);
	result = NULL;

	g_object_unref(proxy);
	proxy = NULL;

	g_object_unref(connection);
	connection = NULL;
}

/* FIXME Poll first, and if device is not present,
 * do not create empty connections. */
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
	
	int reg = 0;
	for (int i = 0; i < NUM_DEV; i++) {
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

				reg++;
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
		g_printerr("No devices were registered.\n");
		return false;
	}

	prop_conn = g_dbus_connection_signal_subscribe(
		connection,
		PROXY_NAME,
		PROPS_NAME,
		PROPS_TYPE,
		PROXY_PATH,
		NULL,
		G_DBUS_SIGNAL_FLAGS_NONE,
		sensor_handler,
		NULL,
		NULL
	);

	return true;
}
