//
// Created by martijn on 26/8/21.
//

#ifndef TEST_ADAPTER_H
#define TEST_ADAPTER_H

#include <gio/gio.h>

int adapter_start_discovery(GDBusConnection *connection, const char *adapterPath);
int adapter_stop_discovery(GDBusConnection *connection, const char *adapterPath);
int set_discovery_filter(GDBusConnection *connection, const char *adapterPath, short rssi_threshold);
int adapter_power_on(GDBusConnection *connection, const char *adapterPath);
int adapter_power_off(GDBusConnection *connection, const char *adapterPath);

#endif //TEST_ADAPTER_H
