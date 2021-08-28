//
// Created by martijn on 26/8/21.
//

#ifndef TEST_ADAPTER_H
#define TEST_ADAPTER_H

#include <gio/gio.h>

typedef struct {
    GDBusConnection *connection;
    const char *path;
    const char *address;
    int powered;
    int discoverable;
    int discovering;
} Adapter;

typedef void (*FindAdaptersCallback) (GPtrArray  *adapters);

GPtrArray* binc_get_adapters();
GPtrArray* binc_find_adapters();
int binc_adapter_start_discovery(Adapter *adapter);
int binc_adapter_stop_discovery(Adapter *adapter);
int binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold);
int binc_adapter_power_on(Adapter *adapter);
int binc_adapter_power_off(Adapter *adapter);

#endif //TEST_ADAPTER_H
