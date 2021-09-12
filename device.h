//
// Created by martijn on 26/8/21.
//

#ifndef TEST_DEVICE_H
#define TEST_DEVICE_H

#include <glib.h>
#include <stdint-gcc.h>
#include "characteristic.h"

typedef struct sDevice Device;

typedef void (*ConnectionStateChangedCallback)(Device *device);

typedef enum ConnectionState {DISCONNECTED = 0, CONNECTED = 1, CONNECTING = 2, DISCONNECTING = 3} ConnectionState;

typedef struct sDevice {
    GDBusConnection *connection;
    const char *adapter_path;
    const char *address;
    const char *address_type;
    const char *alias;
    ConnectionState connection_state;
    gboolean connecting;
    gboolean services_resolved;
    gboolean paired;
    const char *path;
    const char *name;
    short rssi;
    gboolean trusted;
    int txpower;
    GHashTable* manufacturer_data;
    GHashTable* service_data;
    GList* uuids;


    // Internal
    guint device_prop_changed;
    ConnectionStateChangedCallback connection_state_callback;
    ConnectionStateChangedCallback services_resolved_callback;
    GHashTable* services;
    GHashTable* characteristics;
} Device;



Device* binc_create_device(const char* path, GDBusConnection *connection);
void binc_init_device(Device *device);
char* binc_device_to_string(Device *device);
int binc_device_connect(Device *device);
void binc_device_register_connection_state_change_callback(Device *device, ConnectionStateChangedCallback callback);
void binc_device_register_services_resolved_callback(Device *device, ConnectionStateChangedCallback callback);
Characteristic* binc_device_get_characteristic(Device *device, const char* service_uuid, const char* characteristic_uuid);
#endif //TEST_DEVICE_H
