#include <stdio.h>
#include <gio/gio.h>

#include "sensors.h"
#include "ipc-client.h"

#define ROTATION "transform"
#define POWER "power"

extern size_t device_len;
extern char device_cmd[MAX_SHORT_RESP];
extern bool is_light_vendor;

static const char accel_cmd[4][7] = {
	"normal",
	"180",
	"270",
	"90",
};

/**
 * For each orientation and tilt, the first mutually distinct
 * element of all properties is compared, with 'undefined'
 * moved to the end of each list as an edge case.
 */
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

/**
 * Values for proximity sensor. Display power.
 * gboolean near = TRUE = 1 => prox_val[near] -> "off"
 * gboolean near = FALSE = 0 => prox_val[near] -> "on"
 */
static const char prox_val[2][4] = {
	"on",
	"off",
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

	/* ......device_len->|....+10->|....+7 (k=0)->| + '\0' => +18
	 * output {identifier} transform {degree value}
	 *
	 * i <= MAX_PAYLOAD is guaranteed by
	 * MAX_SHORT_RESP + 18 = 96 + 18 = 114 = MAX_PAYLOAD.
	 */
	if (snprintf(cmd, device_len + 18, "%s %s %s",
			device_cmd, ROTATION, accel_cmd[k]) == 0) {
		g_printerr("Failed to write accel payload.\n");
		return;
	}

	if (!ipc_send(RUN_COMMAND, cmd))
		g_printerr("Failed to send command.\n");
}

static void light(const char *key, struct _GVariant *val) {
	printf("Light got %s.\n", key);	
}

static void proximity(const char *key, struct _GVariant *val) {
	char cmd[MAX_PAYLOAD];
	const gboolean near = g_variant_get_boolean(val);

	if (snprintf(cmd, device_len + 12, "%s %s %s",
			device_cmd, POWER, prox_val[near]) == 0) {
		g_printerr("Failed to write proximity payload.\n");
		return;
	}

	if (!ipc_send(RUN_COMMAND, cmd))
		g_printerr("Failed to send command.\n");
}

/**
 * To avoid whole string comparisons, several API-educated guesses
 * are made here and in the callee  branch functions, based on
 * the exact property key string knowledge.
 */
void sensor_handler(
	struct _GDBusConnection *conn,
	const char *sender,
	const char *path,
	const char *interface,
	const char *signal,
	struct _GVariant *data,
	void *
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
		case 'C':
			/* Add compass handler. */
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
