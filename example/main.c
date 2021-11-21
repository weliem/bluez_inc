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
#include "../adapter.h"
#include "../device.h"
#include "../logger.h"
#include "../agent.h"
#include "service_handler_manager.h"
#include "hts_service_handler.h"
#include "dis_service_handler.h"
#include "cts_service_handler.h"
#include "blp_service_handler.h"

#define TAG "Main"
#define CONNECT_DELAY 100

Adapter *default_adapter = NULL;
Agent *agent = NULL;
ServiceHandlerManager *serviceHandlerManager = NULL;

void on_connection_state_changed(Device *device, ConnectionState state, GError *error) {
    if (error != NULL) {
        log_debug(TAG, "(dis)connect failed (error %d: %s)", error->code, error->message);
        return;
    }

    log_debug(TAG, "'%s' (%s) state: %s (%d)", binc_device_get_name(device), binc_device_get_address(device),
              binc_device_get_connection_state_name(device), state);

    // Remove devices immediately of they are not bonded
    if (state == DISCONNECTED && binc_device_get_bonding_state(device) != BONDED ) {
        binc_adapter_remove_device(default_adapter, device);
    }

    if (state == CONNECTED) {
        binc_adapter_start_discovery(default_adapter);
    }
}

void on_bonding_state_changed(Device *device, BondingState new_state, BondingState old_state, GError *error) {
    log_debug(TAG, "bonding state changed from %d to %d", old_state, new_state);
}

void on_notification_state_changed(Characteristic *characteristic, GError *error) {
    const char *service_uuid = binc_characteristic_get_service_uuid(characteristic);
    ServiceHandler *serviceHandler = binc_service_handler_manager_get(serviceHandlerManager, service_uuid);
    if (serviceHandler != NULL && serviceHandler->on_notification_state_updated != NULL) {
        serviceHandler->on_notification_state_updated(
                serviceHandler,
                binc_characteristic_get_device(characteristic),
                characteristic,
                error);
    }
}

void on_notify(Characteristic *characteristic, GByteArray *byteArray) {
    const char *service_uuid = binc_characteristic_get_service_uuid(characteristic);
    ServiceHandler *serviceHandler = binc_service_handler_manager_get(serviceHandlerManager, service_uuid);
    if (serviceHandler != NULL && serviceHandler->on_characteristic_changed != NULL) {
        serviceHandler->on_characteristic_changed(
                serviceHandler,
                binc_characteristic_get_device(characteristic),
                characteristic,
                byteArray,
                NULL);
    }
}

void on_read(Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    const char *service_uuid = binc_characteristic_get_service_uuid(characteristic);
    ServiceHandler *serviceHandler = binc_service_handler_manager_get(serviceHandlerManager, service_uuid);
    if (serviceHandler != NULL && serviceHandler->on_characteristic_changed != NULL) {
        serviceHandler->on_characteristic_changed(
                serviceHandler,
                binc_characteristic_get_device(characteristic),
                characteristic,
                byteArray,
                error);
    }
}

void on_write(Characteristic *characteristic, GError *error) {
    const char *service_uuid = binc_characteristic_get_service_uuid(characteristic);
    ServiceHandler *serviceHandler = binc_service_handler_manager_get(serviceHandlerManager, service_uuid);
    if (serviceHandler != NULL && serviceHandler->on_characteristic_write != NULL) {
        serviceHandler->on_characteristic_write(
                serviceHandler,
                binc_characteristic_get_device(characteristic),
                characteristic,
                NULL,
                error);
    }
}

GList* order_services(GList *services) {
    GList *ordered_services = NULL;
    GList *preferred_order = NULL;

    preferred_order = g_list_append(preferred_order, DIS_SERVICE);
    preferred_order = g_list_append(preferred_order, CTS_SERVICE);

    // Get the services that are in the preferred order list
    for (GList *iterator = preferred_order; iterator; iterator = iterator->next) {
        const char* service_uuid = (char *) iterator->data;
        for (GList *service_iterator = services; service_iterator; service_iterator = service_iterator->next) {
            Service *service = (Service *) service_iterator->data;
            if (g_str_equal(binc_service_get_uuid(service), service_uuid)) {
                ordered_services = g_list_append(ordered_services, service);
            }
        }
    }
    g_list_free(preferred_order);

    // Add the remainder
    for (GList *iterator = services; iterator; iterator = iterator->next) {
        Service *service = (Service *) iterator->data;
        if(g_list_index(ordered_services, service) < 0 ) {
            ordered_services = g_list_append(ordered_services, service);
        }
    }
    return ordered_services;
}

void on_services_resolved(Device *device) {
    log_debug(TAG, "'%s' services resolved", binc_device_get_name(device));

    // Activate service handlers if they exist
    GList *device_services = binc_device_get_services(device);
    GList *services = order_services(device_services);
    for (GList *iterator = services; iterator; iterator = iterator->next) {
        Service *service = (Service *) iterator->data;
        const char* service_uuid = binc_service_get_uuid(service);
        ServiceHandler *serviceHandler = binc_service_handler_manager_get(serviceHandlerManager, service_uuid);
        if (serviceHandler != NULL && serviceHandler->on_characteristics_discovered != NULL) {
            serviceHandler->on_characteristics_discovered(serviceHandler, device);
        }
    }
    g_list_free(services);
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
//    if (name != NULL && g_str_has_prefix(name, "TAIDOC")) {
    binc_adapter_stop_discovery(adapter);

    binc_device_set_connection_state_change_callback(device, &on_connection_state_changed);
    binc_device_set_services_resolved_callback(device, &on_services_resolved);
    binc_device_set_bonding_state_changed_callback(device, &on_bonding_state_changed);
    binc_device_set_read_char_callback(device, &on_read);
    binc_device_set_write_char_callback(device, &on_write);
    binc_device_set_notify_char_callback(device, &on_notify);
    binc_device_set_notify_state_callback(device, &on_notification_state_changed);
    g_timeout_add(CONNECT_DELAY, delayed_connect, device);
//    }
}

void on_discovery_state_changed(Adapter *adapter, DiscoveryState state, GError *error) {
    if (error != NULL) {
        log_debug(TAG, "discovery error (error %d: %s)", error->code, error->message);
        return;
    }
    log_debug(TAG, "discovery '%s' (%s)", binc_adapter_get_discovery_state_name(adapter), binc_adapter_get_path(adapter));
}

void on_powered_state_changed(Adapter *adapter, gboolean state) {
    log_debug(TAG, "powered '%s' (%s)", state ? "on" : "off", binc_adapter_get_path(adapter));
}

gboolean callback(gpointer data) {
    if (agent != NULL) {
        binc_agent_free(agent);
        agent = NULL;
    }

    if (default_adapter != NULL) {
        binc_adapter_free(default_adapter);
        default_adapter = NULL;
    }

    binc_service_handler_manager_free(serviceHandlerManager);
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
        GPtrArray *service_uuids = g_ptr_array_new();
        g_ptr_array_add(service_uuids, HTS_SERVICE_UUID);
        g_ptr_array_add(service_uuids, BLP_SERVICE_UUID);

        // Setup service handlers
        serviceHandlerManager = binc_service_handler_manager_create();
        ServiceHandler *hts_service_handler = hts_service_handler_create();
        ServiceHandler *dis_service_handler = dis_service_handler_create();
        ServiceHandler *cts_service_handler = cts_service_handler_create();
        ServiceHandler *blp_service_handler = blp_service_handler_create();
        binc_service_handler_manager_add(serviceHandlerManager, hts_service_handler);
        binc_service_handler_manager_add(serviceHandlerManager, dis_service_handler);
        binc_service_handler_manager_add(serviceHandlerManager, cts_service_handler);
        binc_service_handler_manager_add(serviceHandlerManager, blp_service_handler);

        // Set discovery callbacks and start discovery
        binc_adapter_set_discovery_callback(default_adapter, &on_scan_result);
        binc_adapter_set_discovery_state_callback(default_adapter, &on_discovery_state_changed);
        binc_adapter_set_discovery_filter(default_adapter, -100, service_uuids);
        g_ptr_array_free(service_uuids, TRUE);
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