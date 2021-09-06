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
#include "utility.h"

#define TAG "Main"

CentralManager *centralManager = NULL;

void on_scan_result(ScanResult *scanResult) {
    char *scanResultString = scan_result_to_string(scanResult);
    log_debug(TAG, scanResultString);
    g_free(scanResultString);
}

void on_discovery_state_changed(Adapter *adapter) {
    log_debug(TAG, "discovery '%s'", adapter->discovering ? "on" : "off");
}

void on_powered_state_changed(Adapter *adapter) {
    log_debug(TAG, "powered '%s'", adapter->powered ? "on" : "off");
}

gboolean callback(gpointer data) {
    g_main_loop_quit((GMainLoop *) data);
    return FALSE;
}

int main(void) {
    guint8 buf[] = {0, 1, 10, 11};
    GByteArray *byteArray = g_byte_array_new_take(buf,4);
    GString *result = g_byte_array_as_hex(byteArray);
    log_debug(TAG, "bytes %s", result->str);
    g_string_free(result, TRUE);
    g_byte_array_free(byteArray, FALSE);

    // Setup mainloop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Find adapters
    GPtrArray *adapters = binc_find_adapters();

    Adapter *adapter = NULL;
    if (adapters->len > 0) {
        // Take the first adapter
        adapter = g_ptr_array_index(adapters, 0);
        log_debug(TAG, "using adapter '%s'", adapter->path);

        binc_adapter_register_powered_state_callback(adapter, &on_powered_state_changed);
        binc_adapter_power_off(adapter);
        binc_adapter_power_on(adapter);

        // Start a scan
        binc_adapter_register_scan_result_callback(adapter, &on_scan_result);
        binc_adapter_register_discovery_state_callback(adapter, &on_discovery_state_changed);
        binc_adapter_set_discovery_filter(adapter, -100);
        binc_adapter_start_discovery(adapter);
    } else {
        log_debug("MAIN", "No adapter found");
    }

    // Bail out after 10 seconds
    g_timeout_add_seconds(30, callback, loop);

    // Start the mainloop
    g_main_loop_run(loop);

    // Stop the scan
    binc_adapter_stop_discovery(adapter);

    // Clean up

    return 0;
}