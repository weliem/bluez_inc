//
// Created by martijn on 26/8/21.
//

#ifndef TEST_ADAPTER_H
#define TEST_ADAPTER_H

#include <gio/gio.h>
#include "scan_result.h"

typedef struct sAdapter Adapter;
typedef void (*AdapterScanResultCallback) (ScanResult  *scanResult);
typedef void (*AdapterDiscoveryStateChangeCallback) (Adapter  *adapter);
typedef void (*AdapterPoweredStateChangeCallback) (Adapter  *adapter);

typedef struct sAdapter {
    GDBusConnection *connection;
    const char *path;
    const char *address;
    int powered;
    int discoverable;
    int discovering;

    guint prop_changed;
    guint iface_added;
    guint iface_removed;

    AdapterScanResultCallback scan_results_callback;
    AdapterDiscoveryStateChangeCallback discoveryStateCallback;
    AdapterPoweredStateChangeCallback poweredStateCallback;
} Adapter;


GPtrArray* binc_get_adapters();
GPtrArray* binc_find_adapters();

int binc_adapter_start_discovery(Adapter *adapter);
int binc_adapter_stop_discovery(Adapter *adapter);
int binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold);
int binc_adapter_power_on(Adapter *adapter);
int binc_adapter_power_off(Adapter *adapter);
void binc_adapter_register_scan_result_callback(Adapter *adapter, AdapterScanResultCallback callback);
void binc_adapter_register_discovery_state_callback(Adapter *adapter, AdapterDiscoveryStateChangeCallback callback);
void binc_adapter_register_powered_state_callback(Adapter *adapter, AdapterPoweredStateChangeCallback callback);

#endif //TEST_ADAPTER_H
