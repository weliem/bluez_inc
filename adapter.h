//
// Created by martijn on 26/8/21.
//

#ifndef TEST_ADAPTER_H
#define TEST_ADAPTER_H

#include <gio/gio.h>
#include "device.h"

typedef struct sAdapter Adapter;

typedef enum DiscoveryState {
    STOPPED = 0, STARTED = 1, STARTING = 2, STOPPING = 3
} DiscoveryState;

typedef void (*AdapterDiscoveryResultCallback)(Adapter *adapter, Device *device);

typedef void (*AdapterDiscoveryStateChangeCallback)(Adapter *adapter);

typedef void (*AdapterPoweredStateChangeCallback)(Adapter *adapter);

typedef struct sAdapter {
    // Public stuff
    char *path;
    char *address;
    gboolean powered;
    gboolean discoverable;
    DiscoveryState discovery_state;

    // Internal stuff
    GDBusConnection *connection;
    guint device_prop_changed;
    guint adapter_prop_changed;
    guint iface_added;
    guint iface_removed;

    AdapterDiscoveryResultCallback discoveryResultCallback;
    AdapterDiscoveryStateChangeCallback discoveryStateCallback;
    AdapterPoweredStateChangeCallback poweredStateCallback;

    GHashTable *devices_cache;
} Adapter;

/**
 * Get the default adapter
 *
 * @return the default adapter or NULL if no adapter was found. Caller owns adapter.
 */
Adapter *binc_get_default_adapter();

GPtrArray *binc_find_adapters();

void binc_adapter_free(Adapter *adapter);

int binc_adapter_start_discovery(Adapter *adapter);

int binc_adapter_stop_discovery(Adapter *adapter);

int binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold);

int binc_adapter_remove_device(Adapter *adapter, Device *device);

int binc_adapter_power_on(Adapter *adapter);

int binc_adapter_power_off(Adapter *adapter);

void binc_adapter_register_discovery_callback(Adapter *adapter, AdapterDiscoveryResultCallback callback);

void binc_adapter_register_discovery_state_callback(Adapter *adapter, AdapterDiscoveryStateChangeCallback callback);

void binc_adapter_register_powered_state_callback(Adapter *adapter, AdapterPoweredStateChangeCallback callback);

#endif //TEST_ADAPTER_H
