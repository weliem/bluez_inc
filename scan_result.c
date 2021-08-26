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
    scanResult->paired = FALSE;
    scanResult->path = NULL;
    scanResult->name = NULL;
    scanResult->rssi = -255;
    scanResult->trusted = 0;
    scanResult->uuids = NULL;
}