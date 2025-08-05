#pragma once

#define NUM_DEV 3

/**
 * Connect to the sensor proxy via GDBus.
 * @return false on error
 */
bool gdbus_connect();

/**
 * Clean up and close connection to GDBus.
 */
void gdbus_close();
