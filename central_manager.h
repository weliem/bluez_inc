//
// Created by martijn on 26/8/21.
//

#ifndef TEST_CENTRAL_MANAGER_H
#define TEST_CENTRAL_MANAGER_H

#include <gio/gio.h>
#include "scan_result.h"
#include "adapter.h"

typedef void (*CentralManagerScanResultCallback) (ScanResult  *scanResult);

typedef struct {
    GDBusConnection *dbusConnection;
    Adapter *adapter;

    CentralManagerScanResultCallback scanResultCallback;
} CentralManager;


CentralManager* binc_create_central_manager(Adapter *adapter);
void binc_close_central_manager(CentralManager *centralManager);

void binc_cm_register_scan_result_callback(CentralManager *centralManager, CentralManagerScanResultCallback callback);
int binc_scan_for_peripherals(CentralManager* centralManager);
int binc_stop_scanning(CentralManager *centralManager);

#endif //TEST_CENTRAL_MANAGER_H
