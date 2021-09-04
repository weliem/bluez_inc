//
// Created by martijn on 26/8/21.
//

#ifndef TEST_SCAN_RESULT_H
#define TEST_SCAN_RESULT_H

#include <glib.h>
#include <stdint-gcc.h>

typedef struct {
    const char *adapter_path;
    const char *address;
    const char *address_type;
    const char *alias;
    int connected;
    const char *interface;
    int paired;
    const char *path;
    const char *name;
    short rssi;
    int trusted;
    int txpower;
    GHashTable* manufacturer_data;
    GHashTable* service_data;
    GList* uuids;
} ScanResult;

void init_scan_result(ScanResult *scanResult);
char* scan_result_to_string(ScanResult *scanResult);

#endif //TEST_SCAN_RESULT_H
