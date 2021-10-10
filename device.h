//
// Created by martijn on 26/8/21.
//

#ifndef TEST_DEVICE_H
#define TEST_DEVICE_H

#include <glib.h>
#include <stdint-gcc.h>
#include "forward_decl.h"
#include "characteristic.h"

typedef void (*ConnectionStateChangedCallback)(Device *device);

typedef enum ConnectionState {
    DISCONNECTED = 0, CONNECTED = 1, CONNECTING = 2, DISCONNECTING = 3
} ConnectionState;

typedef enum BondingState {
    NONE = 0, BONDING = 1, BONDED = 2
} BondingState;

Device *binc_create_device(const char *path, Adapter *adapter);

void binc_init_device(Device *device);

void binc_device_free(Device *device);

GDBusConnection *binc_device_get_dbus_connection(Device *device);

Adapter* binc_device_get_adapter(Device *device);

char *binc_device_to_string(Device *device);

void binc_device_connect(Device *device);

void binc_device_set_read_char_callback(Device *device, OnReadCallback callback);

void binc_device_set_write_char_callback(Device *device, OnWriteCallback callback);

void binc_device_set_notify_char_callback(Device *device, OnNotifyCallback callback);

void binc_device_set_notify_state_callback(Device *device, OnNotifyingStateChangedCallback callback);

void binc_device_register_connection_state_change_callback(Device *device, ConnectionStateChangedCallback callback);

void binc_device_register_services_resolved_callback(Device *device, ConnectionStateChangedCallback callback);

Characteristic *
binc_device_get_characteristic(Device *device, const char *service_uuid, const char *characteristic_uuid);

ConnectionState binc_device_get_connection_state(Device *device);

const char *binc_device_get_address(Device *device);

void binc_device_set_address(Device *device, const char *address);

const char *binc_device_get_address_type(Device *device);

void binc_device_set_address_type(Device *device, const char *address_type);

const char *binc_device_get_alias(Device *device);

void binc_device_set_alias(Device *device, const char *alias);

const char *binc_device_get_adapter_path(Device *device);

void binc_device_set_adapter_path(Device *device, const char *adapter_path);

const char *binc_device_get_name(Device *device);

void binc_device_set_name(Device *device, const char *name);

const char *binc_device_get_path(Device *device);

void binc_device_set_path(Device *device, const char *path);

gboolean binc_device_get_paired(Device *device);

void binc_device_set_paired(Device *device, gboolean paired);

short binc_device_get_rssi(Device *device);

void binc_device_set_rssi(Device *device, short rssi);

gboolean binc_device_get_trusted(Device *device);

void binc_device_set_trusted(Device *device, gboolean trusted);

short binc_device_get_txpower(Device *device);

void binc_device_set_txpower(Device *device, short txpower);

GList *binc_device_get_uuids(Device *device);

void binc_device_set_uuids(Device *device, GList *uuids);

GHashTable *binc_device_get_manufacturer_data(Device *device);

void binc_device_set_manufacturer_data(Device *device, GHashTable *manufacturer_data);

GHashTable *binc_device_get_service_data(Device *device);

void binc_device_set_service_data(Device *device, GHashTable *service_data);

BondingState binc_device_get_bonding_state(Device *device);

void binc_device_set_bonding_state(Device *device, BondingState bonding_state);

#endif //TEST_DEVICE_H
