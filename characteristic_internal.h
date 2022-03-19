//
// Created by martijn on 20-11-21.
//

#ifndef BINC_CHARACTERISTIC_INTERNAL_H
#define BINC_CHARACTERISTIC_INTERNAL_H

#include "characteristic.h"

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

void binc_characteristic_set_uuid(Characteristic *characteristic, const char *uuid);

void binc_characteristic_set_notifying(Characteristic *characteristic, gboolean notifying);

void binc_characteristic_add_descriptor(Characteristic *characteristic, Descriptor *descriptor);

#endif //BINC_CHARACTERISTIC_INTERNAL_H
