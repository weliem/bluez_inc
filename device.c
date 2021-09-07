//
// Created by martijn on 26/8/21.
//

#include "device.h"
#include "utility.h"

void init_device(Device *device) {
    device->adapter_path = NULL;
    device->address = NULL;
    device->address_type = NULL;
    device->alias = NULL;
    device->connected = FALSE;
    device->manufacturer_data = NULL;
    device->name = NULL;
    device->paired = FALSE;
    device->path = NULL;
    device->rssi = -255;
    device->service_data = NULL;
    device->trusted = FALSE;
    device->txpower = 0;
    device->uuids = NULL;
}

Device* create_device(const char* path) {
    Device *x = g_new(Device, 1);
    init_device(x);
    x->path = path;
}

char *device_to_string(Device *device) {

    // First build up uuids string
    GString *uuids = g_string_new("[");
    if (g_list_length(device->uuids) > 0) {
        for (GList *iterator = device->uuids; iterator; iterator = iterator->next) {
            g_string_append_printf(uuids, "%s, ", (char *) iterator->data);
        }
        g_string_truncate(uuids, uuids->len - 2);
    }
    g_string_append(uuids, "]");

    // Build up manufacturer data string
    GString *manufacturer_data = g_string_new("[");
    if (device->manufacturer_data != NULL && g_hash_table_size(device->manufacturer_data) > 0) {
        GHashTableIter iter;
        int *key;
        gpointer value;
        g_hash_table_iter_init(&iter, device->manufacturer_data);
        while (g_hash_table_iter_next(&iter, (gpointer) &key, &value)) {
            GByteArray *byteArray = (GByteArray *) value;
            GString *byteArrayString = g_byte_array_as_hex(byteArray);
            gint keyInt = *key;
            g_string_append_printf(manufacturer_data, "%04X -> %s, ", keyInt, byteArrayString->str);
        }
        g_string_truncate(manufacturer_data, manufacturer_data->len - 2);
    }
    g_string_append(manufacturer_data, "]");

    return g_strdup_printf(
            "Device{name='%s', address='%s', address_type=%s, rssi=%d, uuids=%s, manufacturer_data=%s, paired=%d, txpower=%d path='%s' }",
            device->name,
            device->address,
            device->address_type,
            device->rssi,
            uuids->str,
            manufacturer_data->str,
            device->paired,
            device->txpower,
            device->path
    );
}