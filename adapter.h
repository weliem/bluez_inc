//
// Created by martijn on 26/8/21.
//

#ifndef TEST_ADAPTER_H
#define TEST_ADAPTER_H

#include <gio/gio.h>
#include "forward_decl.h"

typedef enum DiscoveryState {
    STOPPED = 0, STARTED = 1, STARTING = 2, STOPPING = 3
} DiscoveryState;

typedef void (*AdapterDiscoveryResultCallback)(Adapter *adapter, Device *device);

typedef void (*AdapterDiscoveryStateChangeCallback)(Adapter *adapter);

typedef void (*AdapterPoweredStateChangeCallback)(Adapter *adapter);


Adapter *binc_get_default_adapter(GDBusConnection *dbusConnection);

void binc_adapter_free(Adapter *adapter);

void binc_adapter_start_discovery(Adapter *adapter);

void binc_adapter_stop_discovery(Adapter *adapter);

void binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold);

void binc_adapter_remove_device(Adapter *adapter, Device *device);

Device* binc_adapter_get_device_by_path(Adapter *adapter, const char* path);

void binc_adapter_power_on(Adapter *adapter);

void binc_adapter_power_off(Adapter *adapter);

char* binc_adapter_get_path(Adapter *adapter);

DiscoveryState binc_adapter_get_discovery_state(Adapter *adapter);

gboolean binc_adapter_get_powered_state(Adapter *adapter);

void binc_adapter_set_discovery_callback(Adapter *adapter, AdapterDiscoveryResultCallback callback);

void binc_adapter_set_discovery_state_callback(Adapter *adapter, AdapterDiscoveryStateChangeCallback callback);

void binc_adapter_set_powered_state_callback(Adapter *adapter, AdapterPoweredStateChangeCallback callback);

GDBusConnection *binc_adapter_get_dbus_connection(Adapter *adapter);

#endif //TEST_ADAPTER_H
