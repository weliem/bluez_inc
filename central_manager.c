//
// Created by martijn on 26/8/21.
//

#include "central_manager.h"
#include "adapter.h"
#include "logger.h"

const char* TAG = "CentralManager";


void binc_cm_register_scan_result_callback(CentralManager *centralManager, CentralManagerScanResultCallback callback) {
    if (centralManager == NULL) {
        log_info(TAG, "central manager is null");
    }
    if (callback == NULL) {
        log_info(TAG, "scanResultCallback is null");
    }

    centralManager->scanResultCallback = callback;
}

int binc_scan_for_peripherals(CentralManager* centralManager) {
    log_debug(TAG, "starting scan");
    binc_adapter_set_discovery_filter(centralManager->adapter, -100);
    return binc_adapter_start_discovery(centralManager->adapter);
}

int binc_stop_scanning(CentralManager *centralManager) {
    log_debug(TAG, "stopping scan");
    return binc_adapter_stop_discovery(centralManager->adapter);
}



CentralManager* binc_create_central_manager(Adapter *adapter) {
    log_debug(TAG, "creating central manager");
    CentralManager* centralManager = g_new(CentralManager, 1);
    centralManager->dbusConnection = adapter->connection;
    centralManager->adapter = adapter;
    centralManager->scanResultCallback = NULL;

    return centralManager;
}

void binc_close_central_manager(CentralManager *centralManager) {
    log_debug(TAG, "closing central manager");

    g_object_unref(centralManager->dbusConnection);
    g_free(centralManager);
}