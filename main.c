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

#include "uuid.h"
#include "central_manager.h"


void on_scan_result(ScanResult *scanResult) {
    g_print("Address : %s\n", scanResult->address);
    g_print("Alias : %s\n", scanResult->alias);
    g_print("Name : %s\n", scanResult->name);
    g_print("RSSI : %d\n", scanResult->rssi);

    if (g_list_length(scanResult->uuids) > 0) {
        g_print("UUIDs : \n");
        for (GList *iterator = scanResult->uuids; iterator; iterator = iterator->next) {
            g_print("<%s>\n", (char *) iterator->data);
        }
    }
}

gboolean callback(gpointer data) {
    g_main_loop_quit((GMainLoop *) data);
    return FALSE;
}

int main(void) {
    // Setup mainloop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Create CentralManager
    CentralManager *centralManager = create_central_manager();

    // Start a scan
    register_scan_result_callback(centralManager, &on_scan_result);
    scan_for_peripherals(centralManager);

    // Scan for 10 seconds
    g_timeout_add_seconds(10, callback, loop);

    // Start the mainloop
    g_main_loop_run(loop);

    // Stop the scan
    stop_scanning(centralManager);

    // Clean up
    close_central_manager(centralManager);

    return 0;
}