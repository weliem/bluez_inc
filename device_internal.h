//
// Created by martijn on 20-11-21.
//

#ifndef BINC_DEVICE_INTERNAL_H
#define BINC_DEVICE_INTERNAL_H

#include "device.h"

Device *binc_create_device(const char *path, Adapter *adapter);
void binc_device_free(Device *device);
GDBusConnection *binc_device_get_dbus_connection(const Device *device);
void binc_device_set_address(Device *device, const char *address);
void binc_device_set_address_type(Device *device, const char *address_type);
void binc_device_set_alias(Device *device, const char *alias);
void binc_device_set_adapter_path(Device *device, const char *adapter_path);
void binc_device_set_name(Device *device, const char *name);
void binc_device_set_path(Device *device, const char *path);
void binc_device_set_paired(Device *device, gboolean paired);
void binc_device_set_rssi(Device *device, short rssi);
void binc_device_set_trusted(Device *device, gboolean trusted);
void binc_device_set_txpower(Device *device, short txpower);
void binc_device_set_uuids(Device *device, GList *uuids);
void binc_device_set_manufacturer_data(Device *device, GHashTable *manufacturer_data);
void binc_device_set_service_data(Device *device, GHashTable *service_data);
void binc_device_set_bonding_state(Device *device, BondingState bonding_state);
void binc_internal_device_update_property(Device *device, const char *property_name, GVariant *property_value);

#endif //BINC_DEVICE_INTERNAL_H
