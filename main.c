/*
 * bluez_adapter_scan.c - Scan for bluetooth devices
 * 	- This example scans for new devices after powering the adapter, if any devices
 * 	  appeared in /org/hciX/dev_XX_YY_ZZ_AA_BB_CC, it is monitered using "ScanResult"
 *	  signal and all the properties of the device is printed
 *	- Scanning continues to run until any device is disappered, this happens after 180 seconds
 *	  automatically if the device is not used.
 * gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -o bluez_adapter_scan ./bluez_adapter_scan.c `pkg-config --libs glib-2.0 gio-2.0`
 */
#include <glib.h>
#include <gio/gio.h>

#include "adapter.h"
#include "central_manager.h"
#include "logger.h"

const char* MAIN_TAG = "Main";

CentralManager *centralManager = NULL;

void on_scan_result(ScanResult *scanResult) {
    char *scanResultString = scan_result_to_string(scanResult);
    log_debug(MAIN_TAG, scanResultString);
    g_free(scanResultString);

//    g_print("Address : %s\n", scanResult->address);
//    g_print("Alias : %s\n", scanResult->alias);
//    g_print("Name : %s\n", scanResult->name);
//    g_print("RSSI : %d\n", scanResult->rssi);
//
//    if (g_list_length(scanResult->uuids) > 0) {
//        g_print("UUIDs : \n");
//        for (GList *iterator = scanResult->uuids; iterator; iterator = iterator->next) {
//            g_print("<%s>\n", (char *) iterator->data);
//        }
//    }
}



gboolean callback(gpointer data) {
    g_main_loop_quit((GMainLoop *) data);
    return FALSE;
}

int main(void) {
    // Setup mainloop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Find adapters
    GPtrArray *adapters = binc_find_adapters();

    if (adapters->len > 0) {
        // Take the first adapter
        Adapter *adapter = g_ptr_array_index(adapters, 0);
        log_debug("MAIN", adapter->path);

        // Create CentralManager
        centralManager = binc_create_central_manager(adapter);

        // Start a scan
        binc_register_scan_result_callback(centralManager, &on_scan_result);
        binc_scan_for_peripherals(centralManager);
    } else {
        log_debug("MAIN", "No adapter found");
    }

    // Bail out after 10 seconds
    g_timeout_add_seconds(10, callback, loop);

    // Start the mainloop
    g_main_loop_run(loop);

    // Stop the scan
    binc_stop_scanning(centralManager);

    // Clean up
    binc_close_central_manager(centralManager);

    return 0;
}