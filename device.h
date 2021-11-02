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
#ifndef TEST_DEVICE_H
#define TEST_DEVICE_H

#include <glib.h>
#include <stdint-gcc.h>
#include "forward_decl.h"
#include "characteristic.h"

typedef enum ConnectionState {
    DISCONNECTED = 0, CONNECTED = 1, CONNECTING = 2, DISCONNECTING = 3
} ConnectionState;

typedef enum BondingState {
    NONE = 0, BONDING = 1, BONDED = 2
} BondingState;

typedef void (*ConnectionStateChangedCallback)(Device *device, ConnectionState state, GError *error);

typedef void (*ServicesResolvedCallback)(Device *device);

typedef void (*BondingStateChangedCallback)(Device *device, BondingState new_state, BondingState old_state, GError *error);

Device *binc_create_device(const char *path, Adapter *adapter);

void binc_device_free(Device *device);

GDBusConnection *binc_device_get_dbus_connection(Device *device);

Adapter *binc_device_get_adapter(const Device *device);

char *binc_device_to_string(Device *device);

/**
 * Connect to a device asynchronously
 *
 * When a connection is attempted, established or failed, the ConnectionStateChangedCallback is called. Times out in 25 seconds.
 *
 * @param device the device to connect to. Must be non-null and not already connected.
 */
void binc_device_connect(Device *device);

void binc_device_pair(Device *device);

void binc_device_set_read_char_callback(Device *device, OnReadCallback callback);

void binc_device_set_write_char_callback(Device *device, OnWriteCallback callback);

void binc_device_set_notify_char_callback(Device *device, OnNotifyCallback callback);

void binc_device_set_notify_state_callback(Device *device, OnNotifyingStateChangedCallback callback);

void binc_device_set_connection_state_change_callback(Device *device, ConnectionStateChangedCallback callback);

void binc_device_set_services_resolved_callback(Device *device, ServicesResolvedCallback callback);

void binc_device_set_bonding_state_changed_callback(Device *device, BondingStateChangedCallback callback);

GList *binc_device_get_services(Device *device);

Service *binc_device_get_service(Device *device, const char *service_uuid);

Characteristic *
binc_device_get_characteristic(Device *device, const char *service_uuid, const char *characteristic_uuid);

ConnectionState binc_device_get_connection_state(const Device *device);

const char *binc_device_get_connection_state_name(const Device *device);

const char *binc_device_get_address(const Device *device);

void binc_device_set_address(Device *device, const char *address);

const char *binc_device_get_address_type(const Device *device);

void binc_device_set_address_type(Device *device, const char *address_type);

const char *binc_device_get_alias(const Device *device);

void binc_device_set_alias(Device *device, const char *alias);

const char *binc_device_get_adapter_path(Device *device);

void binc_device_set_adapter_path(Device *device, const char *adapter_path);

const char *binc_device_get_name(const Device *device);

void binc_device_set_name(Device *device, const char *name);

const char *binc_device_get_path(const Device *device);

void binc_device_set_path(Device *device, const char *path);

gboolean binc_device_get_paired(const Device *device);

void binc_device_set_paired(Device *device, gboolean paired);

short binc_device_get_rssi(const Device *device);

void binc_device_set_rssi(Device *device, short rssi);

gboolean binc_device_get_trusted(const Device *device);

void binc_device_set_trusted(Device *device, gboolean trusted);

short binc_device_get_txpower(const Device *device);

void binc_device_set_txpower(Device *device, short txpower);

GList *binc_device_get_uuids(const Device *device);

void binc_device_set_uuids(Device *device, GList *uuids);

GHashTable *binc_device_get_manufacturer_data(const Device *device);

void binc_device_set_manufacturer_data(Device *device, GHashTable *manufacturer_data);

GHashTable *binc_device_get_service_data(const Device *device);

void binc_device_set_service_data(Device *device, GHashTable *service_data);

BondingState binc_device_get_bonding_state(const Device *device);

void binc_device_set_bonding_state(Device *device, BondingState bonding_state);

#endif //TEST_DEVICE_H
