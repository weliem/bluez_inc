//
// Created by martijn on 26/8/21.
//

#include "scan_result.h"
#include "utility.h"

void init_scan_result(ScanResult *scanResult) {
    scanResult->adapter_path = NULL;
    scanResult->address = NULL;
    scanResult->address_type = NULL;
    scanResult->alias = NULL;
    scanResult->connected = FALSE;
    scanResult->interface = NULL;
    scanResult->manufacturer_data = NULL;
    scanResult->name = NULL;
    scanResult->paired = FALSE;
    scanResult->path = NULL;
    scanResult->rssi = -255;
    scanResult->service_data = NULL;
    scanResult->trusted = 0;
    scanResult->txpower = 0;
    scanResult->uuids = NULL;
}

char *scan_result_to_string(ScanResult *scanResult) {

    // First build up uuids string
    GString *uuids = g_string_new("[");
    if (g_list_length(scanResult->uuids) > 0) {
        for (GList *iterator = scanResult->uuids; iterator; iterator = iterator->next) {
            g_string_append_printf(uuids, "%s, ", (char *) iterator->data);
        }
        g_string_truncate(uuids, uuids->len -2);
    }
    g_string_append(uuids, "]");

    // Build up manufacturer data string
    GString *manufacturer_data = g_string_new("[");
    if (scanResult->manufacturer_data != NULL && g_hash_table_size(scanResult->manufacturer_data) > 0) {
        GHashTableIter iter;
        int *key;
        gpointer value;
        g_hash_table_iter_init (&iter, scanResult->manufacturer_data);
        while (g_hash_table_iter_next (&iter, (gpointer) &key, &value))
        {
            /* do something with key and value */
            GByteArray *byteArray = (GByteArray*) value;
            GString *byteArrayString = g_byte_array_as_hex(byteArray);
            gint keyInt = *key;
            g_string_append_printf(manufacturer_data, "%04X -> %s, ", keyInt, byteArrayString->str);
        }
        g_string_truncate(manufacturer_data, manufacturer_data->len - 2);
    }
    g_string_append(manufacturer_data, "]");

    return g_strdup_printf("ScanResult{name='%s', address='%s', address_type=%s, rssi=%d, uuids=%s, manufacturer_data=%s, paired=%d, txpower=%d path='%s' }",
                           scanResult->name,
                           scanResult->address,
                           scanResult->address_type,
                           scanResult->rssi,
                           uuids->str,
                           manufacturer_data->str,
                           scanResult->paired,
                           scanResult->txpower,
                           scanResult->path
    );
}