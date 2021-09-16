/*
 * bluez_adapter_scan.c - Scan for bluetooth devices
 * 	- This example scans for new devices after powering the adapter, if any devices
 * 	  appeared in /org/hciX/dev_XX_YY_ZZ_AA_BB_CC, it is monitered using "Device"
 *	  signal and all the properties of the device is printed
 *	- Scanning continues to run until any device is disappered, this happens after 180 seconds
 *	  automatically if the device is not used.
 * gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -o bluez_adapter_scan ./bluez_adapter_scan.c `pkg-config --libs glib-2.0 gio-2.0`
 */
#include <glib.h>
#include <gio/gio.h>

#include "adapter.h"
#include "device.h"
#include "logger.h"
#include "utility.h"
#include "parser.h"

#define TAG "Main"

void on_connection_state_changed(Device *device) {
    log_debug(TAG, "'%s' %s", device->name, device->connection_state ? "connected" : "disconnected");
}

void on_services_resolved(Device *device) {
    log_debug(TAG, "'%s' services resolved", device->name);
//    Characteristic* manufacturer = binc_device_get_characteristic(device, "0000180a-0000-1000-8000-00805f9b34fb", "00002a29-0000-1000-8000-00805f9b34fb");
//    GByteArray *byteArray = binc_characteristic_read(manufacturer);
//    log_debug(TAG, "manufacturer = %s", byteArray->data);

    Characteristic * temperature = binc_device_get_characteristic(device, "00001809-0000-1000-8000-00805f9b34fb","00002a1c-0000-1000-8000-00805f9b34fb" );
    if (temperature != NULL) {
        log_debug(TAG, "starting notify for temperature");
        binc_characteristic_start_notify(temperature);
    }
}

void on_scan_result(Adapter *adapter, Device *device) {
    char *deviceToString = binc_device_to_string(device);
    log_debug(TAG, deviceToString);
    g_free(deviceToString);

    if (!g_strcmp0(device->name, "TAIDOC TD1242")) {
        binc_adapter_stop_discovery(adapter);
        binc_device_register_connection_state_change_callback(device, &on_connection_state_changed);
        binc_device_register_services_resolved_callback(device, &on_services_resolved);
        binc_device_connect(device);
    }
}

void on_discovery_state_changed(Adapter *adapter) {
    log_debug(TAG, "discovery '%s'", adapter->discovery_state? "on" : "off");
}

void on_powered_state_changed(Adapter *adapter) {
    log_debug(TAG, "powered '%s'", adapter->powered ? "on" : "off");
}

gboolean callback(gpointer data) {
    g_main_loop_quit((GMainLoop *) data);
    return FALSE;
}

int main(void) {
//    guint8 buf[] = {0xff, 0x00, 0x01, 0x6c};
    guint8 buf[] = {0x6c, 0x01, 0x00, 0xff};

    GByteArray *byteArray = g_byte_array_new_take(buf,4);
    GString *result = g_byte_array_as_hex(byteArray);
    log_debug(TAG, "bytes %s", result->str);

    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
//    int value1 = parser_get_uint16(parser);
//    int value2 = parser_get_uint16(parser);
//    log_debug(TAG, "value 1: %d, value 2: %d", value1, value2);

    float value = parser_get_float(parser);
    log_debug(TAG, "value %f", value);

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
        binc_adapter_register_discovery_callback(adapter, &on_scan_result);
        binc_adapter_register_discovery_state_callback(adapter, &on_discovery_state_changed);
        binc_adapter_set_discovery_filter(adapter, -100);
        binc_adapter_start_discovery(adapter);
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