/**
 * SPDX-License-Identifier: MIT
 * Sensor handlers: process sensor proxy readings on GDBus callbacks.
 * Copyright (C) 2025 Fuad Veliev <fuad@grrlz.net>
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <gio/gio.h>

#include "sensors.h"
#include "ipc-client.h"

#define BRIGHTNESS "brightnessctl set"

/* Throughout the property/command selection logic, integers
 * (wm_spec, wm_accel, i, k, etc.) are used as array indices to
 * navigate across different properties and their appropriate commands.
 * Booleans are added to them to make final binary choices. */

extern wmenv_t wm_spec;
extern wmaccel_t wm_accel;
extern size_t device_len;
extern char device_cmd[MAX_SHORT_RESP];

/* Generic parts and proximity modifiers for Sway/i3 and Hyprland. */
static const char wm_cmd[5 + 5][32] = {
	/* Sway, i3 */
	"output",
	"transform",
	"sway-tilt-cmd",
	"power on",	
	"power off",
	/* Hyprland */
	"keyword monitor",
	",preferred,auto,auto,transform,",
	"hypr-tilt-cmd",
	",preferred,auto,auto",
	",disable",
};

/* Orientation and tilt modifiers for Sway/i3 and Hyprland. */
static const char accel_cmd[5 + 6 + 5 + 6][12] = {
	/* Sway, i3 */
	"normal",
	"180",
	"270",
	"90",
	"normal",
	"sway-tilt-0",
	"sway-tilt-1",
	"sway-tilt-2",
	"sway-tilt-3",
	"sway-tilt-4",
	"sway-tilt-0",
	/* Hyprland */
	"0",
	"2",
	"1",
	"3",
	"0",
	"hypr-tilt-0",
	"hypr-tilt-1",
	"hypr-tilt-2",
	"hypr-tilt-3",
	"hypr-tilt-4",
	"hypr-tilt-0",
};

/* Mutually distinct chars in property values of accelerometer readings. */
static const char accel_val[5 + 6] = {
	/* Orientation, first (i = 0) element. (k -> 'n') */
	'n',		/* _n_ormal */
	'b',		/* _b_ottom-up */
	'l',		/* _l_eft-up */
	'r',		/* _r_ight-up */
	'u',		/* _u_ndefined */
	/* Tilt, eighth (i = 7) element. (k -> 'l') */	
	'l',		/* vertica_l_ */
	'u',		/* tilted-_u_p */
	'd',		/* tilted-_d_own */
	'\0',		/* face-up_\0_ */
	'w',		/* face-do_w_n */
	'e',		/* undefin_e_d */
};

typedef enum {
	SI_LUX = 1,
	VENDOR = 0,
	UNKNOWN = -1,
} LightUnit;

static LightUnit unit = UNKNOWN;

/* The given values are sample and will be updated during testing,
 * to either an efficient heuristic or by a function at runtime. */
static double light_max[2] = {
	100.0,		/* unit = VENDOR = 0 */
	1200.0,		/* unit = SI_LUX = 1 */
};

static inline void accel(const char *key, struct _GVariant *val) {
	int i = 0, k = 0;
	bool is_tilt = false;
	char cmd[MAX_PAYLOAD];
	const char *prop = g_variant_get_string(val, NULL);

	if (key[13] == 'T') {
		i = 7;
		k = 5;
		is_tilt = true;
		return; /* Remove when defining commands for tilt. */
	}

	while (prop[i] != accel_val[k])
		k++;

	if (snprintf(cmd,
		device_len + 2*sizeof(wm_cmd[0]) + sizeof(accel_cmd[0]),
		"%s %s %s %s",
		wm_cmd[wm_spec],
		device_cmd,
		wm_cmd[wm_spec + 1 + is_tilt],
		accel_cmd[wm_accel + k]) == 0)
	{
		g_printerr("[Accelerometer] Could not write payload.\n");
		return;
	}

	if (!ipc_send(RUN_COMMAND, cmd))
		g_printerr("[Accelerometer] IO socket operation failed.\n");
}

/* Since a{sv} is iterated over inside the generic handler,
 * either light unit or level will be passed here, but not both. */
static inline void light(const char *key, struct _GVariant *val) {
	/*...LightLevel_U_nit */
	if (key[10] == 'U') {
		if (unit == UNKNOWN) {
			const char *u = g_variant_get_string(val, NULL);
			unit = (strchr(u, 'l')) ? SI_LUX : VENDOR;
		} 
		return;
	}

	char cmd[MAX_PAYLOAD];
	double level = g_variant_get_double(val);
	unsigned int percentage = (unsigned int)(level * 100.0 / light_max[unit]);

	if (snprintf(cmd,
		sizeof(BRIGHTNESS) + 3 + sizeof("%") + 1,
		"%s %d%%",
		BRIGHTNESS, 
		percentage) == 0)
	{
		g_printerr("[Ambient light] Could not write payload.\n");
		return;
	}

	if (!ipc_send(RUN_COMMAND, cmd))
		g_printerr("[Ambient light] IO socket operation failed.\n");
}

static inline void proximity(const char *key, struct _GVariant *val) {
	char cmd[MAX_PAYLOAD];
	const gboolean near = g_variant_get_boolean(val);

	if (snprintf(cmd,
		device_len + 2*sizeof(wm_cmd[0]),
		"%s %s %s",
		wm_cmd[wm_spec],
		device_cmd,
		wm_cmd[wm_spec + 3 + near]) == 0)
	{
		g_printerr("[Proximity] Could not write payload.\n");
		return;
	}

	if (!ipc_send(RUN_COMMAND, cmd))
		g_printerr("[Proximity] IO socket operation failed.\n");
}

void sensor_handler(
	struct _GDBusConnection *conn,
	const char *sender,
	const char *path,
	const char *interface,
	const char *signal,
	struct _GVariant *data,
	void * user_data
) {
	/* str and iter_null are only used
	 * for g_variant_get() to split the data properly. */	
	const char *str;
	const char *key;	
	GVariant *val;	
	g_autoptr(GVariantIter) iter_dict = NULL;
	g_autoptr(GVariantIter) iter_null = NULL;

	g_variant_get(data, "(sa{sv}as)", &str, &iter_dict, &iter_null);

	while (g_variant_iter_next(iter_dict, "{&sv}", &key, &val)) {
		/* Match the first letter of a keyword. */
		switch (key[0]) {
		case 'A':
			accel(key, val);
			break;
		case 'L':
			light(key, val);
			break;
		case 'P':
			proximity(key, val);
			break;
		case 'H':
			/* Ignore Has{Device} property.
			 * FIXME Check it in gdbus_connect() instead. */
			break;
		default:
			g_printerr("Unmatched property: %s.\n", key);
		}
	}

	g_variant_unref(val);	
}
