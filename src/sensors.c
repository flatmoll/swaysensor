#include <stdio.h>
#include <gio/gio.h>

#include "sensors.h"
#include "ipc-client.h"

#define ROTATION "transform"

extern char device_cmd[MAX_SHORT_RESP];

const char accel_cmd[4][7] = {
	"normal",
	"180",
	"270",
	"90",
};

static void accel(const char *key, struct _GVariant *val) {
	int k = 0;
	char cmd[MAX_PAYLOAD];
	const char *prop = g_variant_get_string(val, NULL);

	/*               13
	 * Accelerometer_O_rientation 
	 * Accelerometer_T_ilt */
	if (key[13] == 'O') {
		/* Match the first element of a property. */
		switch (prop[0]) {
		case 'n':
			/* k = 0; */
			break;
		case 'b':
			k = 1;
			break;
		case 'l':
			k = 2;
			break;
		case 'r':
			k = 3;
			break;
		default:				/* Undefined */
			break;
		}
	} else if (key[13] == 'T') {
		/* For tilt, eighth element is mutually distinct. */
		switch (prop[7]) {
		case 'l':			/* vertica_l_ */
			break;
		case 'u':			/* tilted-_u_p */
			break;
		case 'd':			/* tilted-_d_own */
			break;
		case '\0':			/* face-up_\0_ */
			break;
		case 'w':			/* face-do_w_n */
			break;
		default:				/* Undefined */
			break;
		}
		printf("Tilt placeholder.\n");
		return;
	} else {
		fprintf(stderr, "Debug unknown accel.\n");
		return;
	}


	/* ......device_len->|....+10->|....+7 (k=0)->| + '\0' => +18
	 * output {identifier} transform {degree value}
	 */
	if (snprintf(cmd, device_len + 18, "%s %s %s", device_cmd,
				ROTATION, accel_cmd[k]) > MAX_PAYLOAD) {
		fprintf(stderr, "Maximum payload length exceeded.\n");
		return;
	}

	if (!ipc_send(RUN_COMMAND, cmd))
		fprintf(stderr, "Could not send command or \
				the response was unsuccessful.\n");
}

static void light(const char *key, struct _GVariant *val) {
	printf("Light got %s.\n", key);	
}

static void proximity(const char *key, struct _GVariant *val) {
	printf("Proximity got %s.\n", key);	
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
			fprintf(stderr, "Unmatched property: %s.\n", key);
		}
	}

	g_variant_unref(val);	
}
