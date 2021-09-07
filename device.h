//
// Created by martijn on 26/8/21.
//

#ifndef TEST_DEVICE_H
#define TEST_DEVICE_H

#include <glib.h>
#include <stdint-gcc.h>

typedef struct {
    const char *adapter_path;
    const char *address;
    const char *address_type;
    const char *alias;
    gboolean connected;
    gboolean paired;
    const char *path;
    const char *name;
    short rssi;
    gboolean trusted;
    int txpower;
    GHashTable* manufacturer_data;
    GHashTable* service_data;
    GList* uuids;
} Device;

Device* create_device(const char* path);
void init_device(Device *device);
char* device_to_string(Device *device);

#endif //TEST_DEVICE_H
