//
// Created by martijn on 26/8/21.
//

#ifndef TEST_ADAPTER_H
#define TEST_ADAPTER_H

#include <gio/gio.h>
#include "device.h"

typedef struct sAdapter Adapter;
typedef void (*AdapterDiscoveryResultCallback) (Device  *device);
typedef void (*AdapterDiscoveryStateChangeCallback) (Adapter  *adapter);
typedef void (*AdapterPoweredStateChangeCallback) (Adapter  *adapter);

typedef struct sAdapter {
    GDBusConnection *connection;
    const char *path;
    const char *address;
    gboolean powered;
    gboolean discoverable;
    gboolean discovering;

    guint device_prop_changed;
    guint adapter_prop_changed;
    guint iface_added;
    guint iface_removed;

    AdapterDiscoveryResultCallback discoveryResultCallback;
    AdapterDiscoveryStateChangeCallback discoveryStateCallback;
    AdapterPoweredStateChangeCallback poweredStateCallback;

    GHashTable* devices_cache;
} Adapter;


GPtrArray* binc_find_adapters();
int binc_adapter_start_discovery(Adapter *adapter);
int binc_adapter_stop_discovery(Adapter *adapter);
int binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold);
int binc_adapter_power_on(Adapter *adapter);
int binc_adapter_power_off(Adapter *adapter);
void binc_adapter_register_discovery_callback(Adapter *adapter, AdapterDiscoveryResultCallback callback);
void binc_adapter_register_discovery_state_callback(Adapter *adapter, AdapterDiscoveryStateChangeCallback callback);
void binc_adapter_register_powered_state_callback(Adapter *adapter, AdapterPoweredStateChangeCallback callback);

#endif //TEST_ADAPTER_H
