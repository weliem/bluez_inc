//
// Created by martijn on 26/8/21.
//

#ifndef TEST_CENTRAL_MANAGER_H
#define TEST_CENTRAL_MANAGER_H

#include <gio/gio.h>
#include "scan_result.h"

typedef void (*CentralManagerScanResultCallback) (ScanResult  *scanResult);

typedef struct {
    GDBusConnection *dbusConnection;
    const char *adapterPath;
    guint prop_changed;
    guint iface_added;
    guint iface_removed;
    CentralManagerScanResultCallback scanResultCallback;
} CentralManager;


CentralManager* create_central_manager();
void close_central_manager(CentralManager *centralManager);

void register_scan_result_callback(CentralManager *centralManager, CentralManagerScanResultCallback callback);
int scan_for_peripherals(CentralManager* centralManager);
int stop_scanning(CentralManager *centralManager);

#endif //TEST_CENTRAL_MANAGER_H
