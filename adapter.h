/*
 *   Copyright (c) 2021 Martijn van Welie
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 *
 */

#ifndef BINC_ADAPTER_H
#define BINC_ADAPTER_H

#include <gio/gio.h>
#include "forward_decl.h"

typedef enum DiscoveryState {
    STOPPED = 0, STARTED = 1, STARTING = 2, STOPPING = 3
} DiscoveryState;

typedef void (*AdapterDiscoveryResultCallback)(Adapter *adapter, Device *device);

typedef void (*AdapterDiscoveryStateChangeCallback)(Adapter *adapter, DiscoveryState state, const GError *error);

typedef void (*AdapterPoweredStateChangeCallback)(Adapter *adapter, gboolean state);

/**
 * Get the default bluetooth adapter
 *
 * @param dbusConnection the dbus connection
 * @return an adapter object or NULL if no adapters were found. Must be freed using binc_adapter_free().
 */
Adapter *binc_get_default_adapter(GDBusConnection *dbusConnection);

void binc_adapter_free(Adapter *adapter);

void binc_adapter_start_discovery(Adapter *adapter);

void binc_adapter_stop_discovery(Adapter *adapter);

void binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold, GPtrArray *service_uuids);

void binc_adapter_remove_device(Adapter *adapter, Device *device);

Device *binc_adapter_get_device_by_path(Adapter *adapter, const char *path);

void binc_adapter_power_on(Adapter *adapter);

void binc_adapter_power_off(Adapter *adapter);

const char *binc_adapter_get_path(const Adapter *adapter);

DiscoveryState binc_adapter_get_discovery_state(const Adapter *adapter);

const char* binc_adapter_get_discovery_state_name(const Adapter *adapter);

gboolean binc_adapter_get_powered_state(const Adapter *adapter);

void binc_adapter_set_discovery_callback(Adapter *adapter, AdapterDiscoveryResultCallback callback);

void binc_adapter_set_discovery_state_callback(Adapter *adapter, AdapterDiscoveryStateChangeCallback callback);

void binc_adapter_set_powered_state_callback(Adapter *adapter, AdapterPoweredStateChangeCallback callback);

GDBusConnection *binc_adapter_get_dbus_connection(const Adapter *adapter);

#endif //BINC_ADAPTER_H
