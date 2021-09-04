//
// Created by martijn on 26/8/21.
//

#include "scan_result.h"

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

    return g_strdup_printf("ScanResult{name='%s', address='%s', address_type=%s, rssi=%d, uuids=%s, paired=%d, txpower=%d path='%s' }",
                           scanResult->name,
                           scanResult->address,
                           scanResult->address_type,
                           scanResult->rssi,
                           uuids->str,
                           scanResult->paired,
                           scanResult->txpower,
                           scanResult->path
    );
}