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
#include "adapter.h"
#include "device.h"
#include "logger.h"
#include "utility.h"
#include "parser.h"
#include "agent.h"

#define TAG "Main"

#define NONIN "Nonin3230_502644076"
#define SOEHNLE "Systo MC 400"
#define TAIDOC "TAIDOC TD1241"

#define CONNECT_DELAY 100


#define DIS_SERVICE "0000180a-0000-1000-8000-00805f9b34fb"
#define MANUFACTURER_CHAR "00002a29-0000-1000-8000-00805f9b34fb"
#define MODEL_CHAR "00002a24-0000-1000-8000-00805f9b34fb"

void on_connection_state_changed(Device *device) {
    log_debug(TAG, "'%s' %s", device->name, device->connection_state ? "connected" : "disconnected");
}

void on_notify(Characteristic *characteristic, GByteArray *byteArray) {
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    parser->offset = 1;
    float value = parser_get_float(parser);
    log_debug(TAG, "temperature %.1f", value);
    parser_free(parser);
}

void on_notify_bpm(Characteristic *characteristic, GByteArray *byteArray) {
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    parser->offset = 1;
    float systolic = parser_get_sfloat(parser);
    float diastolic = parser_get_sfloat(parser);
    log_debug(TAG, "bpm %.0f/%.0f", systolic, diastolic);
    parser_free(parser);
}

void on_read(Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    if (byteArray != NULL) {
        if (g_str_equal(characteristic->uuid, MANUFACTURER_CHAR)) {
            log_debug(TAG, "manufacturer = %s", byteArray->data);
        } else if (g_str_equal(characteristic->uuid, MODEL_CHAR)) {
            log_debug(TAG, "model = %s", byteArray->data);
        }
    }

    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", characteristic->uuid, error->code, error->message);
        g_clear_error(&error);
    }
}

void on_write(Characteristic *characteristic, GError *error) {
    if (error != NULL) {
        log_debug(TAG, "failed to write '%s' (error %d: %s)", characteristic->uuid, error->code, error->message);
        g_clear_error(&error);
    } else {
        log_debug(TAG, "writing succeeded");
    }
}

void on_services_resolved(Device *device) {
    log_debug(TAG, "'%s' services resolved", device->name);
    Characteristic *manufacturer = binc_device_get_characteristic(device, DIS_SERVICE, MANUFACTURER_CHAR);
    if (manufacturer != NULL) {
        binc_characteristic_set_read_callback(manufacturer, &on_read);
        binc_characteristic_read(manufacturer);
    }

    Characteristic *model = binc_device_get_characteristic(device, DIS_SERVICE, MODEL_CHAR);
    if (model != NULL) {
        binc_characteristic_set_read_callback(model, &on_read);
        binc_characteristic_read(model);
    }

    Characteristic * temperature = binc_device_get_characteristic(device, "00001809-0000-1000-8000-00805f9b34fb","00002a1c-0000-1000-8000-00805f9b34fb" );
    if (temperature != NULL) {
        log_debug(TAG, "starting notify for temperature");
        binc_characteristic_set_notify_callback(temperature,  &on_notify);
        binc_characteristic_start_notify(temperature);
    }

    Characteristic *current_time = binc_device_get_characteristic(device, "00001805-0000-1000-8000-00805f9b34fb","00002a2b-0000-1000-8000-00805f9b34fb" );
    if (current_time != NULL) {
        GByteArray *timeBytes = binc_get_current_time();
        log_debug(TAG, "writing current time" );
        binc_characteristic_set_write_callback(current_time, &on_write);
        binc_characteristic_write(current_time, timeBytes, WITH_RESPONSE);
    }

    Characteristic *bpm = binc_device_get_characteristic(device, "00001810-0000-1000-8000-00805f9b34fb", "00002a35-0000-1000-8000-00805f9b34fb");
    if (bpm != NULL) {
        binc_characteristic_set_notify_callback(bpm, &on_notify_bpm);
        binc_characteristic_start_notify(bpm);
    }
}

gboolean delayed_connect(gpointer device) {
    binc_device_connect((Device *) device);
    return FALSE;
}

void on_scan_result(Adapter *adapter, Device *device) {
    char *deviceToString = binc_device_to_string(device);
    log_debug(TAG, deviceToString);
    g_free(deviceToString);

    if (device->name != NULL && g_str_has_prefix(device->name, "Philips")) {
        binc_adapter_stop_discovery(adapter);
        binc_device_register_connection_state_change_callback(device, &on_connection_state_changed);
        binc_device_register_services_resolved_callback(device, &on_services_resolved);
        g_timeout_add(CONNECT_DELAY, delayed_connect, device);
    }
}


void on_discovery_state_changed(Adapter *adapter) {
    log_debug(TAG, "discovery '%s'", adapter->discovery_state ? "on" : "off");
}

void on_powered_state_changed(Adapter *adapter) {
    log_debug(TAG, "powered '%s'", adapter->powered ? "on" : "off");
}

gboolean callback(gpointer data) {
    g_main_loop_quit((GMainLoop *) data);
    return FALSE;
}

int main(void) {
    Agent *agent = NULL;

    // Setup mainloop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Get the default adapter
    Adapter *adapter = binc_get_default_adapter();

    if (adapter != NULL) {
        log_debug(TAG, "using adapter '%s'", adapter->path);

        agent = binc_agent_create(adapter, KEYBOARD_DISPLAY);

        binc_adapter_register_powered_state_callback(adapter, &on_powered_state_changed);
        binc_adapter_power_off(adapter);
        binc_adapter_power_on(adapter);

        // Start a scan
        binc_adapter_register_discovery_callback(adapter, &on_scan_result);
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
    if (agent != NULL) {
        binc_agent_free(agent);
    }

    return 0;
}