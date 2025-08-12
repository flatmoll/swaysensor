#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <gio/gio.h>

#include "sensors.h"
#include "ipc-client.h"

#define BRIGHTNESS "brightnessctl set"

extern unsigned int wm_spec;
extern unsigned int wm_accel;
extern size_t device_len;
extern char device_cmd[MAX_SHORT_RESP];

static const char wm_cmd[8][32] = {
	/* Sway, i3 */
	"output",
	"transform",
	"power on",	
	"power off",
	/* Hyprland */
	"keyword monitor",
	",preferred,auto,auto,transform,",
	",preferred,auto,auto",
	",disable",	
};

// keyword monitor {ID},preferred,auto,auto,transform,{0,2,1,3}
static const char accel_cmd[22][12] = {
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

/* For both orientation and tilt, the first mutually distinct
 * element of all properties is compared, while 'undefined'
 * is moved to the end of each list as an edge case. */
static const char accel_val[5 + 6] = {
	/* Orientation, first element. (i = 0, k -> 'n') */
	'n',		/* _n_ormal */
	'b',		/* _b_ottom-up */
	'l',		/* _l_eft-up */
	'r',		/* _r_ight-up */
	'u',		/* _u_ndefined */
	/* Tilt, eighth element. (i = 7, k -> 'l') */	
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

static void accel(const char *key, struct _GVariant *val) {
	int i = 0, k = 0;
	char cmd[MAX_PAYLOAD];
	const char *prop = g_variant_get_string(val, NULL);

	if (key[13] == 'T') {
		i = 7;
		k = 5;
		return; /* Change cmd instead when implementing tilt. */
	}

	while (prop[i] != accel_val[k]) k++;

	if (snprintf(cmd,
		device_len + 2*sizeof(wm_cmd[0]) + sizeof(accel_cmd[0]),
		"%s %s %s %s",
		wm_cmd[wm_spec],
		device_cmd,
		wm_cmd[wm_spec + 1],
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
static void light(const char *key, struct _GVariant *val) {
	if (unit == UNKNOWN) {
		if (key[10] == 'U') { /* LightLevel_U_nit */
			const char *u = g_variant_get_string(val, NULL);
			unit = (strchr(u, 'l')) ? SI_LUX : VENDOR;
		} else return;
	}

	char cmd[MAX_PAYLOAD];
	double level = g_variant_get_double(val);
	unsigned int percentage =
		(unsigned int)(level * 100.0 / light_max[unit]);

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

static void proximity(const char *key, struct _GVariant *val) {
	char cmd[MAX_PAYLOAD];
	const gboolean near = g_variant_get_boolean(val);

	if (snprintf(cmd,
		device_len + 2*sizeof(wm_cmd[0]),
		"%s %s %s",
		wm_cmd[wm_spec],
		device_cmd,
		wm_cmd[wm_spec + 2 + near]) == 0)
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
			/* Ignore HasDevice property, because the
			 * underlying proxy would simply not send
			 * updates if the sensor is unavailable. */
			break;
		default:
			g_printerr("Unmatched property: %s.\n", key);
		}
	}

	g_variant_unref(val);	
}
