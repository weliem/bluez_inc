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
#include  <signal.h>
#include "../adapter.h"
#include "../device.h"
#include "../logger.h"
#include "../agent.h"
#include "../application.h"
#include "service_handler_manager.h"
#include "services/hts_service_handler.h"
#include "services/dis_service_handler.h"
#include "services/cts_service_handler.h"
#include "services/blp_service_handler.h"
#include "device_info.h"
#include "services/wss_service_handler.h"
#include "services/plx_service_handler.h"
#include "services/glx_service_handler.h"
#include "../advertisement.h"
#include "../utility.h"
#include "../parser.h"

#define TAG "Main"
#define CONNECT_DELAY 100

GMainLoop *loop = NULL;
Adapter *default_adapter = NULL;
Agent *agent = NULL;
ServiceHandlerManager *serviceHandlerManager = NULL;
Advertisement *advertisement = NULL;
Application *application = NULL;

void on_connection_state_changed(Device *device, ConnectionState state, const GError *error) {
    if (error != NULL) {
        log_debug(TAG, "(dis)connect failed (error %d: %s)", error->code, error->message);
        return;
    }

    log_debug(TAG, "'%s' (%s) state: %s (%d)", binc_device_get_name(device), binc_device_get_address(device),
              binc_device_get_connection_state_name(device), state);

    if (state == DISCONNECTED) {
        binc_service_handler_manager_device_disconnected(serviceHandlerManager, device);

        // Remove devices immediately of they are not bonded
        if (binc_device_get_bonding_state(device) != BONDED) {
            binc_adapter_remove_device(default_adapter, device);
        }
    }
}

void on_bonding_state_changed(Device *device, BondingState new_state, BondingState old_state, const GError *error) {
    log_debug(TAG, "bonding state changed from %d to %d", old_state, new_state);
}

void on_notification_state_changed(Characteristic *characteristic, const GError *error) {
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

void on_notify(Characteristic *characteristic, const GByteArray *byteArray) {
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

void on_read(Characteristic *characteristic, const GByteArray *byteArray, const GError *error) {
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

void on_write(Characteristic *characteristic, const GByteArray *byteArray, const GError *error) {
    const char *service_uuid = binc_characteristic_get_service_uuid(characteristic);
    ServiceHandler *serviceHandler = binc_service_handler_manager_get(serviceHandlerManager, service_uuid);
    if (serviceHandler != NULL && serviceHandler->on_characteristic_write != NULL) {
        serviceHandler->on_characteristic_write(
                serviceHandler,
                binc_characteristic_get_device(characteristic),
                characteristic,
                byteArray,
                error);
    }
}

void on_desc_read(Descriptor *descriptor, const GByteArray *byteArray, const GError *error) {
    log_debug(TAG, "on descriptor read");
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    GString *parsed_string = parser_get_string(parser);
    log_debug(TAG, "CUD %s", parsed_string->str);
    parser_free(parser);
}

GList *order_services(GList *services) {
    GList *ordered_services = NULL;
    GList *preferred_order = NULL;

    preferred_order = g_list_append(preferred_order, DIS_SERVICE);
    preferred_order = g_list_append(preferred_order, CTS_SERVICE);

    // Get the services that are in the preferred order list
    for (GList *iterator = preferred_order; iterator; iterator = iterator->next) {
        const char *service_uuid = (char *) iterator->data;
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
        if (g_list_index(ordered_services, service) < 0) {
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
        const char *service_uuid = binc_service_get_uuid(service);
        ServiceHandler *serviceHandler = binc_service_handler_manager_get(serviceHandlerManager, service_uuid);
        if (serviceHandler != NULL && serviceHandler->on_characteristics_discovered != NULL) {
            serviceHandler->on_characteristics_discovered(serviceHandler, device,
                                                          binc_service_get_characteristics(service));
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


    binc_device_set_connection_state_change_callback(device, &on_connection_state_changed);
    binc_device_set_services_resolved_callback(device, &on_services_resolved);
    binc_device_set_bonding_state_changed_callback(device, &on_bonding_state_changed);
    binc_device_set_read_char_callback(device, &on_read);
    binc_device_set_write_char_callback(device, &on_write);
    binc_device_set_notify_char_callback(device, &on_notify);
    binc_device_set_notify_state_callback(device, &on_notification_state_changed);
    binc_device_set_read_desc_cb(device, &on_desc_read);
    binc_device_connect(device);
//    g_timeout_add(CONNECT_DELAY, delayed_connect, device);
//    }
}

void on_discovery_state_changed(Adapter *adapter, DiscoveryState state, const GError *error) {
    if (error != NULL) {
        log_debug(TAG, "discovery error (error %d: %s)", error->code, error->message);
        return;
    }
    log_debug(TAG, "discovery '%s' (%s)", binc_adapter_get_discovery_state_name(adapter),
              binc_adapter_get_path(adapter));
}

void on_powered_state_changed(Adapter *adapter, gboolean state) {
    log_debug(TAG, "powered '%s' (%s)", state ? "on" : "off", binc_adapter_get_path(adapter));
}

void on_local_char_read(const Application *application, const char *address, const char* service_uuid, const char* char_uuid) {
    if (g_str_equal(service_uuid, HTS_SERVICE_UUID) && g_str_equal(char_uuid, TEMPERATURE_CHAR_UUID)) {
        const guint8 bytes[] = {0x06, 0x6f, 0x01, 0x00, 0xff, 0xe6, 0x07, 0x03, 0x03, 0x10, 0x04, 0x00, 0x01};
        GByteArray *byteArray = g_byte_array_sized_new(sizeof(bytes));
        g_byte_array_append(byteArray, bytes, sizeof(bytes));
        binc_application_set_char_value(application, service_uuid, char_uuid, byteArray);
    }
}

char* on_local_char_write(const Application *application, const char *address, const char *service_uuid,
                          const char *char_uuid, GByteArray *byteArray) {
    return NULL;
}

void on_local_char_start_notify(const Application *application, const char* service_uuid, const char* char_uuid) {
    log_debug(TAG, "on start notify");
    if (g_str_equal(service_uuid, HTS_SERVICE_UUID) && g_str_equal(char_uuid, TEMPERATURE_CHAR_UUID)) {
        const guint8 bytes[] = {0x06, 0x6A, 0x01, 0x00, 0xff, 0xe6, 0x07, 0x03, 0x03, 0x10, 0x04, 0x00, 0x01};
        GByteArray *byteArray = g_byte_array_sized_new(sizeof(bytes));
        g_byte_array_append(byteArray, bytes, sizeof(bytes));
        binc_application_notify(application, service_uuid, char_uuid, byteArray);
        g_byte_array_free(byteArray, TRUE);
    }
}

void on_local_char_stop_notify(const Application *application, const char* service_uuid, const char* char_uuid) {
    log_debug(TAG, "on stop notify");
}

gboolean callback(gpointer data) {
    if (application != NULL) {
        binc_adapter_unregister_application(default_adapter, application);
        binc_application_free(application);
        application = NULL;
    }

    if (advertisement != NULL) {
        binc_adapter_stop_advertising(default_adapter, advertisement);
        binc_advertisement_free(advertisement);
    }

    if (agent != NULL) {
        binc_agent_free(agent);
        agent = NULL;
    }

    if (default_adapter != NULL) {
        binc_adapter_free(default_adapter);
        default_adapter = NULL;
    }

    binc_service_handler_manager_free(serviceHandlerManager);

    device_info_close();
    g_main_loop_quit((GMainLoop *) data);
    return FALSE;
}

static void cleanup_handler(int signo) {
    if (signo == SIGINT) {
        g_print("received SIGINT\n");
        callback(loop);
    }
}

int main(void) {
    // Get a DBus connection
    GDBusConnection *dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);

    if (signal(SIGINT, cleanup_handler) == SIG_ERR)
        g_print("can't catch SIGINT\n");

    // Setup mainloop
    loop = g_main_loop_new(NULL, FALSE);

    // Get the default default_adapter
    default_adapter = binc_get_default_adapter(dbusConnection);

    if (default_adapter != NULL) {
        log_debug(TAG, "using default_adapter '%s'", binc_adapter_get_path(default_adapter));

        // Start advertising
        GPtrArray *adv_service_uuids = g_ptr_array_new();
        g_ptr_array_add(adv_service_uuids, HTS_SERVICE_UUID);
        g_ptr_array_add(adv_service_uuids, BLP_SERVICE_UUID);

        const guint8 bytes[] = {1, 0x02, 0x03, 0x04, 0x05};
        GByteArray *byteArray = g_byte_array_new();
        g_byte_array_append(byteArray, bytes, 5);

        const guint8 service_data[] = {5, 0x02, 0x03};
        GByteArray *service_bytes = g_byte_array_new();
        g_byte_array_append(service_bytes, service_data, 3);

        advertisement = binc_advertisement_create();
        binc_advertisement_set_local_name(advertisement, "BINC2");
        binc_advertisement_set_services(advertisement, adv_service_uuids);
        binc_advertisement_set_manufacturer_data(advertisement, 52, byteArray);
        binc_advertisement_set_service_data(advertisement, HTS_SERVICE_UUID, service_bytes);
        binc_adapter_start_advertising(default_adapter, advertisement);
        g_byte_array_free(byteArray, TRUE);
        g_byte_array_free(service_bytes, TRUE);
        g_ptr_array_free(adv_service_uuids, TRUE);

        // Start application
        application = binc_create_application(default_adapter);
        binc_application_add_service(application, HTS_SERVICE_UUID);
        binc_application_add_characteristic(
                application,
                HTS_SERVICE_UUID,
                TEMPERATURE_CHAR_UUID,
                GATT_CHR_PROP_READ | GATT_CHR_PROP_INDICATE | GATT_CHR_PROP_WRITE);
        binc_application_set_char_read_cb(application, &on_local_char_read);
        binc_application_set_char_write_cb(application, &on_local_char_write);
        binc_application_set_char_start_notify_cb(application, &on_local_char_start_notify);
        binc_application_set_char_stop_notify_cb(application, &on_local_char_stop_notify);
        binc_adapter_register_application(default_adapter, application);

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
        g_ptr_array_add(service_uuids, WSS_SERVICE_UUID);
        g_ptr_array_add(service_uuids, PLX_SERVICE_UUID);
        g_ptr_array_add(service_uuids, GLX_SERVICE_UUID);

        // Setup service handlers
        serviceHandlerManager = binc_service_handler_manager_create();
        ServiceHandler *hts_service_handler = hts_service_handler_create();
        ServiceHandler *dis_service_handler = dis_service_handler_create();
        ServiceHandler *cts_service_handler = cts_service_handler_create();
        ServiceHandler *blp_service_handler = blp_service_handler_create();
        ServiceHandler *wss_service_handler = wss_service_handler_create();
        ServiceHandler *plx_service_handler = plx_service_handler_create();
        ServiceHandler *glx_service_handler = glx_service_handler_create();
        binc_service_handler_manager_add(serviceHandlerManager, hts_service_handler);
        binc_service_handler_manager_add(serviceHandlerManager, dis_service_handler);
        binc_service_handler_manager_add(serviceHandlerManager, cts_service_handler);
        binc_service_handler_manager_add(serviceHandlerManager, blp_service_handler);
        binc_service_handler_manager_add(serviceHandlerManager, wss_service_handler);
        binc_service_handler_manager_add(serviceHandlerManager, plx_service_handler);
        binc_service_handler_manager_add(serviceHandlerManager, glx_service_handler);

        // Set discovery callbacks and start discovery
        binc_adapter_set_discovery_callback(default_adapter, &on_scan_result);
        binc_adapter_set_discovery_state_callback(default_adapter, &on_discovery_state_changed);
        binc_adapter_set_discovery_filter(default_adapter, -100, service_uuids);
        g_ptr_array_free(service_uuids, TRUE);
        binc_adapter_start_discovery(default_adapter);

        //binc_adapter_stop_advertising(default_adapter, advertisement);
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