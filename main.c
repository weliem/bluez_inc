/*
 *   Copyright (c) 2021 Martijn van Welie
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 *
 */

#include <glib.h>
#include <stdio.h>
#include "adapter.h"
#include "device.h"
#include "logger.h"
#include "parser.h"
#include "agent.h"

#define TAG "Main"
#define CONNECT_DELAY 100

#define DIS_SERVICE "0000180a-0000-1000-8000-00805f9b34fb"
#define MANUFACTURER_CHAR "00002a29-0000-1000-8000-00805f9b34fb"
#define MODEL_CHAR "00002a24-0000-1000-8000-00805f9b34fb"

#define HTS_SERVICE "00001809-0000-1000-8000-00805f9b34fb"
#define TEMPERATURE_CHAR "00002a1c-0000-1000-8000-00805f9b34fb"

#define CTS_SERVICE "00001805-0000-1000-8000-00805f9b34fb"
#define CURRENT_TIME_CHAR "00002a2b-0000-1000-8000-00805f9b34fb"

#define BLP_SERVICE "00001810-0000-1000-8000-00805f9b34fb"
#define BLOODPRESSURE_CHAR "00002a35-0000-1000-8000-00805f9b34fb"

Adapter *default_adapter = NULL;
Agent *agent = NULL;

void on_connection_state_changed(Device *device, ConnectionState state, GError *error) {
    if (error != NULL) {
        log_debug(TAG, "(dis)connect failed (error %d: %s)", error->code, error->message);
        return;
    }

    log_debug(TAG, "'%s' (%s) state: %s (%d)", binc_device_get_name(device), binc_device_get_address(device),
              binc_device_get_connection_state_name(device), state);
    if (state == DISCONNECTED && binc_device_get_bonding_state(device) != BONDED) {
        binc_adapter_remove_device(default_adapter, device);
    }
}

void on_notification_state_changed(Characteristic *characteristic, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to start/stop notify '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    gboolean is_notifying = binc_characteristic_is_notifying(characteristic);
    log_debug(TAG, "notification state <%s> : %s", binc_characteristic_get_uuid(characteristic),
              is_notifying ? "true" : "false");
}

void on_notify(Characteristic *characteristic, GByteArray *byteArray) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    parser_set_offset(parser, 1);

    if (g_str_equal(uuid, TEMPERATURE_CHAR)) {
        float value = parser_get_float(parser);
        log_debug(TAG, "temperature %.1f", value);
    } else if (g_str_equal(uuid, BLOODPRESSURE_CHAR)) {
        float systolic = parser_get_sfloat(parser);
        float diastolic = parser_get_sfloat(parser);
        log_debug(TAG, "bpm %.0f/%.0f", systolic, diastolic);
    }
    parser_free(parser);
}

void on_read(Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (byteArray != NULL) {
        Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
        if (g_str_equal(uuid, MANUFACTURER_CHAR)) {
            GString *manufacturer = parser_get_string(parser);
            log_debug(TAG, "manufacturer = %s", manufacturer->str);
            g_string_free(manufacturer, TRUE);
        } else if (g_str_equal(uuid, MODEL_CHAR)) {
            GString *model = parser_get_string(parser);
            log_debug(TAG, "model = %s", model->str);
            g_string_free(model, TRUE);
        }
        parser_free(parser);
    }
}

void on_write(Characteristic *characteristic, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to write '%s' (error %d: %s)", uuid, error->code, error->message);
    } else {
        log_debug(TAG, "writing succeeded");
    }
}

void on_services_resolved(Device *device) {
    log_debug(TAG, "'%s' services resolved", binc_device_get_name(device));
    Characteristic *manufacturer = binc_device_get_characteristic(device, DIS_SERVICE, MANUFACTURER_CHAR);
    if (manufacturer != NULL) {
        binc_characteristic_read(manufacturer);
    }

    Characteristic *model = binc_device_get_characteristic(device, DIS_SERVICE, MODEL_CHAR);
    if (model != NULL) {
        binc_characteristic_read(model);
    }

    Characteristic *temperature = binc_device_get_characteristic(device, HTS_SERVICE, TEMPERATURE_CHAR);
    if (temperature != NULL) {
        log_debug(TAG, "starting notify for temperature");
        binc_characteristic_start_notify(temperature);
    }

    Characteristic *current_time = binc_device_get_characteristic(device, CTS_SERVICE, CURRENT_TIME_CHAR);
    if (current_time != NULL) {
        if (binc_characteristic_supports_write(current_time, WITH_RESPONSE)) {
            GByteArray *timeBytes = binc_get_current_time();
            log_debug(TAG, "writing current time");
            binc_characteristic_write(current_time, timeBytes, WITH_RESPONSE);
            g_byte_array_free(timeBytes, TRUE);
        }
        if (binc_characteristic_supports_notify(current_time)) {
            binc_characteristic_start_notify(current_time);
        }
    }

    Characteristic *bpm = binc_device_get_characteristic(device, BLP_SERVICE, BLOODPRESSURE_CHAR);
    if (bpm != NULL && binc_characteristic_supports_notify(bpm)) {
        binc_characteristic_start_notify(bpm);
    }
}

gboolean on_request_authorization(Device *device) {
    log_debug(TAG, "requesting authorization for '%s", binc_device_get_name(device));
    return TRUE;
}

guint32 on_request_passkey(Device *device) {
    guint32 pass = 000000;
    log_debug(TAG, "requesting passkey for '%s", binc_device_get_name(device));
    log_debug(TAG, "Enter 6 digit pin code: ");
    int result = fscanf(stdin, "%d", &pass);
    if (result != 1) {
        log_debug(TAG, "didn't read a pin code");
    }
    return pass;
}

gboolean delayed_connect(gpointer device) {
    binc_device_connect((Device *) device);
    return FALSE;
}

void on_scan_result(Adapter *adapter, Device *device) {
    char *deviceToString = binc_device_to_string(device);
    log_debug(TAG, deviceToString);
    g_free(deviceToString);

    const char *name = binc_device_get_name(device);
    if (name != NULL && g_str_has_prefix(name, "TAIDOC")) {
        binc_adapter_stop_discovery(adapter);

        binc_device_set_connection_state_change_callback(device, &on_connection_state_changed);
        binc_device_set_services_resolved_callback(device, &on_services_resolved);
        binc_device_set_read_char_callback(device, &on_read);
        binc_device_set_write_char_callback(device, &on_write);
        binc_device_set_notify_char_callback(device, &on_notify);
        binc_device_set_notify_state_callback(device, &on_notification_state_changed);
        g_timeout_add(CONNECT_DELAY, delayed_connect, device);
    }
}

void on_discovery_state_changed(Adapter *adapter, DiscoveryState state, GError *error) {
    if (error != NULL) {
        log_debug(TAG, "discovery error (error %d: %s)", error->code, error->message);
        return;
    }
    log_debug(TAG, "discovery '%s' (%s)", state ? "on" : "off", binc_adapter_get_path(adapter));
}

void on_powered_state_changed(Adapter *adapter, gboolean state) {
    log_debug(TAG, "powered '%s' (%s)", state ? "on" : "off", binc_adapter_get_path(adapter));
}

gboolean callback(gpointer data) {
    binc_agent_free(agent);
    agent = NULL;
    binc_adapter_free(default_adapter);
    default_adapter = NULL;
    g_main_loop_quit((GMainLoop *) data);
    return FALSE;
}

int main(void) {
    // Get a DBus connection
    GDBusConnection *dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);

    // Setup mainloop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Get the default default_adapter
    default_adapter = binc_get_default_adapter(dbusConnection);

    if (default_adapter != NULL) {
        log_debug(TAG, "using default_adapter '%s'", binc_adapter_get_path(default_adapter));

        // Register an agent and set callbacks
        agent = binc_agent_create(default_adapter, "/org/bluez/BincAgent", KEYBOARD_DISPLAY);
        binc_agent_set_request_authorization_callback(agent, &on_request_authorization);
        binc_agent_set_request_passkey_callback(agent, &on_request_passkey);

        // Make sure the adapter is on
        binc_adapter_set_powered_state_callback(default_adapter, &on_powered_state_changed);
        if (!binc_adapter_get_powered_state(default_adapter)) {
            binc_adapter_power_on(default_adapter);
        }

        // Build UUID array so we can use it in the discovery filter
//        GPtrArray *service_uuids = g_ptr_array_new ();
//        g_ptr_array_add(service_uuids, HTS_SERVICE);
//        g_ptr_array_add(service_uuids, BLP_SERVICE);

        // Set discovery callbacks and start discovery
        binc_adapter_set_discovery_callback(default_adapter, &on_scan_result);
        binc_adapter_set_discovery_state_callback(default_adapter, &on_discovery_state_changed);
        binc_adapter_set_discovery_filter(default_adapter, -100, NULL);
        // g_ptr_array_free(service_uuids, TRUE);
        binc_adapter_start_discovery(default_adapter);
    } else {
        log_debug("MAIN", "No default_adapter found");
    }

    // Bail out after some time
    g_timeout_add_seconds(60, callback, loop);

    // Start the mainloop
    g_main_loop_run(loop);

    // Clean up mainloop
    g_main_loop_unref(loop);

    // Disconnect from DBus
    g_dbus_connection_close_sync(dbusConnection, NULL, NULL);
    g_object_unref(dbusConnection);
    return 0;
}