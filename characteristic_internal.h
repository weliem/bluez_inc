//
// Created by martijn on 20-11-21.
//

#ifndef TEST_CHARACTERISTIC_INTERNAL_H
#define TEST_CHARACTERISTIC_INTERNAL_H

/**
 * Create a characteristic
 *
 * @param connection dbus connection
 * @param path the path of the object on the dbus
 * @return the newly create characteristic. Free by calling binc_characteristic_free()
 */
Characteristic *binc_characteristic_create(Device *device, const char *path);

void binc_characteristic_free(Characteristic *characteristic);

void binc_characteristic_set_read_callback(Characteristic *characteristic, OnReadCallback callback);

void binc_characteristic_set_write_callback(Characteristic *characteristic, OnWriteCallback callback);

void binc_characteristic_set_notify_callback(Characteristic *characteristic, OnNotifyCallback callback);

void binc_characteristic_set_notifying_state_change_callback(Characteristic *characteristic,
                                                             OnNotifyingStateChangedCallback callback);

void binc_characteristic_set_service_uuid(Characteristic *characteristic, const char *service_uuid);

void binc_characteristic_set_service_path(Characteristic *characteristic, const char *service_path);

void binc_characteristic_set_flags(Characteristic *characteristic, GList *flags);

void binc_characteristic_set_properties(Characteristic *characteristic, guint properties);

void binc_characteristic_set_uuid(Characteristic *characteristic, const char *uuid);

#endif //TEST_CHARACTERISTIC_INTERNAL_H
