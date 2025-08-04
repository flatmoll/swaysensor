#pragma once

/**
 * Number of devices.
 */
#define NUM_DEV 3

/**
 * For linking purposes, defined in main.c.
 */
extern bool devices[];

/**
 * Connect to the sensor proxy via GDBus.
 * @return false on error
 */
bool gdbus_connect();

/**
 * Clean up and close connection to GDBus.
 */
void gdbus_close();
