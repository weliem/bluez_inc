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

#include <gio/gio.h>
#include "logger.h"
#include "device.h"
#include "utility.h"
#include "service.h"
#include "characteristic.h"
#include "adapter.h"

#define TAG "Device"

char connection_state_names[4][16] =
        {"DISCONNECTED",
         "CONNECTED",
         "CONNECTING",
         "DISCONNECTING"
        };

struct binc_device {
    GDBusConnection *connection;
    Adapter *adapter;
    const char *address;
    const char *address_type;
    const char *alias;
    ConnectionState connection_state;
    gboolean connecting;
    gboolean services_resolved;
    gboolean paired;
    BondingState bondingState;
    const char *path;
    const char *name;
    short rssi;
    gboolean trusted;
    short txpower;
    GHashTable *manufacturer_data;
    GHashTable *service_data;
    GList *uuids;

    // Internal
    guint device_prop_changed;
    ConnectionStateChangedCallback connection_state_callback;
    ServicesResolvedCallback services_resolved_callback;
    BondingStateChangedCallback bonding_state_callback;
    GHashTable *services;
    GList *services_list;
    GHashTable *characteristics;

    OnReadCallback on_read_callback;
    OnWriteCallback on_write_callback;
    OnNotifyCallback on_notify_callback;
    OnNotifyingStateChangedCallback on_notify_state_callback;
};

static void binc_init_device(Device *device) {
    device->adapter = NULL;
    device->address = NULL;
    device->address_type = NULL;
    device->alias = NULL;
    device->bondingState = NONE;
    device->bonding_state_callback = NULL;
    device->connection_state = DISCONNECTED;
    device->connection_state_callback = NULL;
    device->connection = NULL;
    device->manufacturer_data = NULL;
    device->name = NULL;
    device->paired = FALSE;
    device->path = NULL;
    device->rssi = -255;
    device->service_data = NULL;
    device->services_resolved_callback = NULL;
    device->trusted = FALSE;
    device->txpower = 0;
    device->uuids = NULL;
    device->services = NULL;
    device->services_list = NULL;
    device->on_notify_state_callback = NULL;
    device->on_notify_callback = NULL;
    device->on_read_callback = NULL;
    device->on_write_callback = NULL;
}

Device *binc_create_device(const char *path, Adapter *adapter) {
    Device *device = g_new0(Device, 1);
    binc_init_device(device);
    device->path = g_strdup(path);
    device->adapter = adapter;
    device->connection = binc_adapter_get_dbus_connection(adapter);
    return device;
}

void binc_device_free_uuids(Device *device) {
    if (device->uuids != NULL) {
        for (GList *iterator = device->uuids; iterator; iterator = iterator->next) {
            g_free((char *) iterator->data);
        }
        g_list_free(device->uuids);
    }
    device->uuids = NULL;
}

void binc_device_free_manufacturer_data(Device *device) {
    if (device->manufacturer_data != NULL) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, device->manufacturer_data);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_free(key);
            GByteArray *byteArray = (GByteArray *) value;
            g_byte_array_free(byteArray, TRUE);
        }
        g_hash_table_destroy(device->manufacturer_data);
    }
    device->manufacturer_data = NULL;
}

void binc_device_free_service_data(Device *device) {
    if (device->service_data != NULL) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, device->service_data);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_free(key);
            GByteArray *byteArray = (GByteArray *) value;
            g_byte_array_free(byteArray, TRUE);
        }
        g_hash_table_destroy(device->service_data);
    }
    device->service_data = NULL;
}

void binc_device_free(Device *device) {
    g_assert(device != NULL);

    log_debug(TAG, "freeing %s", device->path);

    // Unsubscribe properties changed if needed
    if (device->device_prop_changed != 0) {
        g_dbus_connection_signal_unsubscribe(device->connection, device->device_prop_changed);
        device->device_prop_changed = 0;
    }

    // Free strings
    g_free((char *) device->path);
    device->path = NULL;
    g_free((char *) device->address_type);
    device->address_type = NULL;
    g_free((char *) device->address);
    device->address = NULL;
    g_free((char *) device->alias);
    device->alias = NULL;
    g_free((char *) device->name);
    device->name = NULL;

    // Free characteristics hash table
    if (device->characteristics != NULL) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, device->characteristics);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_free(key);
            Characteristic *characteristic = (Characteristic *) value;
            binc_characteristic_free(characteristic);
        }
        g_hash_table_destroy(device->characteristics);
    }
    device->characteristics = NULL;

    // Free services hash table
    if (device->services != NULL) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, device->services);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_free(key);
            Service *service = (Service *) value;
            binc_service_free(service);
        }
        g_hash_table_destroy(device->services);
    }
    device->services = NULL;

    // Free manufacturer data hash table
    binc_device_free_manufacturer_data(device);

    // Free service data hash table
    binc_device_free_service_data(device);

    // Free uuids
    binc_device_free_uuids(device);

    // Free services list
    if (device->services_list != NULL) {
        g_list_free(device->services_list);
    }
    device->services_list = NULL;

    g_free(device);
}

char *binc_device_to_string(Device *device) {

    // First build up uuids string
    GString *uuids = g_string_new("[");
    if (g_list_length(device->uuids) > 0) {
        for (GList *iterator = device->uuids; iterator; iterator = iterator->next) {
            g_string_append_printf(uuids, "%s, ", (char *) iterator->data);
        }
        g_string_truncate(uuids, uuids->len - 2);
    }
    g_string_append(uuids, "]");

    // Build up manufacturer data string
    GString *manufacturer_data = g_string_new("[");
    if (device->manufacturer_data != NULL && g_hash_table_size(device->manufacturer_data) > 0) {
        GHashTableIter iter;
        int *key;
        gpointer value;
        g_hash_table_iter_init(&iter, device->manufacturer_data);
        while (g_hash_table_iter_next(&iter, (gpointer) &key, &value)) {
            GByteArray *byteArray = (GByteArray *) value;
            GString *byteArrayString = g_byte_array_as_hex(byteArray);
            gint keyInt = *key;
            g_string_append_printf(manufacturer_data, "%04X -> %s, ", keyInt, byteArrayString->str);
            g_string_free(byteArrayString, TRUE);
        }
        g_string_truncate(manufacturer_data, manufacturer_data->len - 2);
    }
    g_string_append(manufacturer_data, "]");

    // Build up service data string
    GString *service_data = g_string_new("[");
    if (device->service_data != NULL && g_hash_table_size(device->service_data) > 0) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, device->service_data);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            GByteArray *byteArray = (GByteArray *) value;
            GString *byteArrayString = g_byte_array_as_hex(byteArray);
            g_string_append_printf(service_data, "%s -> %s, ", (char *) key, byteArrayString->str);
            g_string_free(byteArrayString, TRUE);
        }
        g_string_truncate(service_data, service_data->len - 2);
    }
    g_string_append(service_data, "]");

    char *result = g_strdup_printf(
            "Device{name='%s', address='%s', address_type=%s, rssi=%d, uuids=%s, manufacturer_data=%s, service_data=%s, paired=%d, txpower=%d path='%s' }",
            device->name,
            device->address,
            device->address_type,
            device->rssi,
            uuids->str,
            manufacturer_data->str,
            service_data->str,
            device->paired,
            device->txpower,
            device->path
    );

    g_string_free(uuids, TRUE);
    g_string_free(manufacturer_data, TRUE);
    g_string_free(service_data, TRUE);
    return result;
}

static void binc_on_characteristic_read(Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    Device *device = binc_characteristic_get_device(characteristic);
    if (device->on_read_callback != NULL) {
        device->on_read_callback(characteristic, byteArray, error);
    }
}

static void binc_on_characteristic_write(Characteristic *characteristic, GError *error) {
    Device *device = binc_characteristic_get_device(characteristic);
    if (device->on_write_callback != NULL) {
        device->on_write_callback(characteristic, error);
    }
}

static void binc_on_characteristic_notify(Characteristic *characteristic, GByteArray *byteArray) {
    Device *device = binc_characteristic_get_device(characteristic);
    if (device->on_notify_callback != NULL) {
        device->on_notify_callback(characteristic, byteArray);
    }
}

static void binc_on_characteristic_notification_state_changed(Characteristic *characteristic, GError *error) {
    Device *device = binc_characteristic_get_device(characteristic);
    if (device->on_notify_state_callback != NULL) {
        device->on_notify_state_callback(characteristic, error);
    }
}

static void binc_device_internal_set_conn_state(Device *device, ConnectionState state, GError *error) {
    device->connection_state = state;
    if (device->connection_state_callback != NULL) {
        device->connection_state_callback(device, state, error);
    }
}

static void binc_internal_gatt_tree(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    Device *device = (Device *) user_data;
    g_assert(device != NULL);

    GVariant *result = g_dbus_connection_call_finish(device->connection, res, &error);

    if (result == NULL) {
        g_print("Unable to get result for GetManagedObjects\n");
        if (error != NULL) {
            log_debug(TAG, "call failed (error %d: %s)", error->code, error->message);
            g_clear_error(&error);
            return;
        }
    }

    /* Parse the result */
    GVariantIter iter;
    const gchar *object_path;
    GVariant *ifaces_and_properties;
    if (result) {
        device->services = g_hash_table_new(g_str_hash, g_str_equal);
        device->characteristics = g_hash_table_new(g_str_hash, g_str_equal);
        GVariant *innerResult = g_variant_get_child_value(result, 0);
        g_variant_iter_init(&iter, innerResult);
        while (g_variant_iter_loop(&iter, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties)) {
            if (g_str_has_prefix(object_path, device->path)) {
                const gchar *interface_name;
                GVariant *properties;
                GVariantIter iter2;
                g_variant_iter_init(&iter2, ifaces_and_properties);
                while (g_variant_iter_loop(&iter2, "{&s@a{sv}}", &interface_name, &properties)) {
                    if(g_str_equal(interface_name, "org.bluez.GattService1" )) {
                        char *uuid = NULL;
                        const gchar *property_name;
                        GVariantIter iter3;
                        GVariant *prop_val;
                        g_variant_iter_init(&iter3, properties);
                        while (g_variant_iter_loop(&iter3, "{&sv}", &property_name, &prop_val)) {
                            if (g_strcmp0(property_name, "UUID") == 0) {
                                uuid = g_strdup(g_variant_get_string(prop_val, NULL));
                            }
                        }
                        Service *service = binc_service_create(device, object_path, uuid);
                        g_hash_table_insert(device->services, g_strdup(object_path), service);
                        g_free(uuid);
                    } else if(g_str_equal(interface_name, "org.bluez.GattCharacteristic1" )) {
                        Characteristic *characteristic = binc_characteristic_create(device, object_path);
                        binc_characteristic_set_read_callback(characteristic, &binc_on_characteristic_read);
                        binc_characteristic_set_write_callback(characteristic, &binc_on_characteristic_write);
                        binc_characteristic_set_notify_callback(characteristic, &binc_on_characteristic_notify);
                        binc_characteristic_set_notifying_state_change_callback(characteristic,
                                                                                &binc_on_characteristic_notification_state_changed);

                        const gchar *property_name;
                        GVariantIter iter3;
                        GVariant *prop_val;
                        g_variant_iter_init(&iter3, properties);
                        while (g_variant_iter_loop(&iter3, "{&sv}", &property_name, &prop_val)) {
                            if (g_str_equal(property_name, "UUID")) {
                                binc_characteristic_set_uuid(characteristic, g_variant_get_string(prop_val, NULL));
                            } else if (g_str_equal(property_name, "Service")) {
                                binc_characteristic_set_service_path(characteristic,
                                                                     g_variant_get_string(prop_val, NULL));
                            } else if (g_str_equal(property_name, "Flags")) {
                                binc_characteristic_set_flags(characteristic, g_variant_string_array_to_list(prop_val));
                            }
                        }

                        // Get service and link the characteristic to the service
                        Service *service = g_hash_table_lookup(device->services,
                                                               binc_characteristic_get_service_path(characteristic));
                        if (service != NULL) {
                            binc_service_add_characteristic(service, characteristic);
                            binc_characteristic_set_service_uuid(characteristic, binc_service_get_uuid(service));
                            g_hash_table_insert(device->characteristics, g_strdup(object_path), characteristic);

                            char *charString = binc_characteristic_to_string(characteristic);
                            log_debug(TAG, charString);
                            g_free(charString);
                        } else {
                            log_debug(TAG, "could not find service %s",
                                      binc_characteristic_get_service_path(characteristic));
                        }
                    }
                }
            }
        }
        g_variant_unref(innerResult);
        g_variant_unref(result);
    }

    device->services_list = g_hash_table_get_values(device->services);
    log_debug(TAG, "found %d services", g_list_length(device->services_list));
    if (device->services_resolved_callback != NULL) {
        device->services_resolved_callback(device);
    }
}

static void binc_collect_gatt_tree(Device *device) {
    g_assert(device != NULL);

    g_dbus_connection_call(device->connection,
                           "org.bluez",
                           "/",
                           "org.freedesktop.DBus.ObjectManager",
                           "GetManagedObjects",
                           NULL,
                           G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_gatt_tree,
                           device);
}

static void binc_device_changed(GDBusConnection *conn,
                                const gchar *sender,
                                const gchar *path,
                                const gchar *interface,
                                const gchar *signal,
                                GVariant *params,
                                void *userdata) {

    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    const char *iface;
    const char *key;
    GVariant *value = NULL;
    const gchar *signature = g_variant_get_type_string(params);

    Device *device = (Device *) userdata;
    g_assert(device != NULL);

    if (g_strcmp0(signature, "(sa{sv}as)") != 0) {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        goto done;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while (g_variant_iter_loop(properties, "{&sv}", &key, &value)) {
        if (g_str_equal(key, "Connected")) {
            binc_device_internal_set_conn_state(device, g_variant_get_boolean(value), NULL);
            if (device->connection_state == DISCONNECTED) {
                // Unsubscribe properties changed signal
                g_dbus_connection_signal_unsubscribe(device->connection, device->device_prop_changed);
                device->device_prop_changed = 0;
            }
        } else if (g_str_equal(key, "ServicesResolved")) {
            device->services_resolved = g_variant_get_boolean(value);
            log_debug(TAG, "ServicesResolved %s", device->services_resolved ? "true" : "false");
            if (device->services_resolved == TRUE && device->bondingState != BONDING) {
                binc_collect_gatt_tree(device);
            }

            if (device->services_resolved == FALSE) {
                binc_device_internal_set_conn_state(device, DISCONNECTING, NULL);
            }
        } else if (g_str_equal(key, "Paired")) {
            device->paired = g_variant_get_boolean(value);
            log_debug(TAG, "Paired %s", device->paired ? "true" : "false");
            binc_device_set_bonding_state(device, device->paired ? BONDED : NONE);

            // If gatt-tree has not been built yet, start building it
            if (device->services == NULL && device->services_resolved) {
                binc_collect_gatt_tree(device);
            }
        }
    }

    done:
    if (properties != NULL)
        g_variant_iter_free(properties);
    if (unknown != NULL)
        g_variant_iter_free(unknown);
}

static void binc_internal_device_connect(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    Device *device = (Device *) user_data;
    g_assert(device != NULL);

    GVariant *value = g_dbus_connection_call_finish(device->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "Connect failed (error %d: %s)", error->code, error->message);
        binc_device_internal_set_conn_state(device, DISCONNECTED, error);
        g_clear_error(&error);
    }

    if (value != NULL) {
        g_variant_unref(value);
    }
}

void subscribe_prop_changed(Device *device) {
    if (device->device_prop_changed == 0) {
        device->device_prop_changed = g_dbus_connection_signal_subscribe(device->connection,
                                                                         "org.bluez",
                                                                         "org.freedesktop.DBus.Properties",
                                                                         "PropertiesChanged",
                                                                         device->path,
                                                                         "org.bluez.Device1",
                                                                         G_DBUS_SIGNAL_FLAGS_NONE,
                                                                         binc_device_changed,
                                                                         device,
                                                                         NULL);
    }
}

void binc_device_connect(Device *device) {
    g_assert(device != NULL);
    g_assert(device->path != NULL);

    // Don't do anything if we are not disconnected
    if (device->connection_state != DISCONNECTED) return;

    log_debug(TAG, "Connecting to '%s' (%s) (%s)", device->name, device->address,
              device->paired ? "BONDED" : "BOND NONE");

    binc_device_internal_set_conn_state(device, CONNECTING, NULL);
    subscribe_prop_changed(device);
    g_dbus_connection_call(device->connection,
                           "org.bluez",
                           device->path,
                           "org.bluez.Device1",
                           "Connect",
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_device_connect,
                           device);
}

static void binc_internal_device_pair(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    Device *device = (Device *) user_data;
    g_assert(device != NULL);

    GVariant *value = g_dbus_connection_call_finish(device->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "Pair", error->code, error->message);
        binc_device_internal_set_conn_state(device, DISCONNECTED, error);
        g_clear_error(&error);
    }

    if (value != NULL) {
        g_variant_unref(value);
    }
}

void binc_device_pair(Device *device) {
    g_assert(device != NULL);
    log_debug(TAG, "pairing device");

    if (device->connection_state == DISCONNECTING) {
        return;
    }

    if (device->connection_state == DISCONNECTED) {
        binc_device_internal_set_conn_state(device, CONNECTING, NULL);
    }

    subscribe_prop_changed(device);
    g_dbus_connection_call(device->connection,
                           "org.bluez",
                           device->path,
                           "org.bluez.Device1",
                           "Pair",
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_device_pair,
                           device);
}

static void binc_internal_device_disconnect(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    Device *device = (Device *) user_data;
    g_assert(device != NULL);

    GVariant *value = g_dbus_connection_call_finish(device->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "Disconnect", error->code, error->message);
        binc_device_internal_set_conn_state(device, CONNECTED, error);
        g_clear_error(&error);
    }

    if (value != NULL) {
        g_variant_unref(value);
    }
}

void binc_device_disconnect(Device *device) {
    g_assert(device != NULL);
    g_assert(device->path != NULL);

    // Don't do anything if we are not connected
    if (device->connection_state != CONNECTED) return;

    log_debug(TAG, "Disconnecting '%s' (%s)", device->name, device->address);

    binc_device_internal_set_conn_state(device, DISCONNECTING, NULL);
    g_dbus_connection_call(device->connection,
                           "org.bluez",
                           device->path,
                           "org.bluez.Device1",
                           "Disconnect",
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_device_disconnect,
                           device);
}

void binc_device_set_connection_state_change_callback(Device *device, ConnectionStateChangedCallback callback) {
    g_assert(device != NULL);
    g_assert(callback != NULL);

    device->connection_state_callback = callback;
}

GList *binc_device_get_services(Device *device) {
    g_assert(device != NULL);
    return device->services_list;
}

void binc_device_set_services_resolved_callback(Device *device, ServicesResolvedCallback callback) {
    g_assert(device != NULL);
    g_assert(callback != NULL);

    device->services_resolved_callback = callback;
}

void binc_device_set_bonding_state_changed_callback(Device *device, BondingStateChangedCallback callback) {
    g_assert(device != NULL);
    g_assert(callback != NULL);

    device->bonding_state_callback = callback;
}

Service *binc_device_get_service(Device *device, const char *service_uuid) {
    g_assert(device != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));

    if (device->services_list != NULL) {
        for (GList *iterator = device->services_list; iterator; iterator = iterator->next) {
            Service *service = (Service *) iterator->data;
            if (g_str_equal(service_uuid, binc_service_get_uuid(service))) {
                return service;
            }
        }
    }

    return NULL;
}

Characteristic *
binc_device_get_characteristic(Device *device, const char *service_uuid, const char *characteristic_uuid) {
    g_assert(device != NULL);
    g_assert(service_uuid != NULL);
    g_assert(characteristic_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));
    g_assert(g_uuid_string_is_valid(characteristic_uuid));

    Service *service = binc_device_get_service(device, service_uuid);
    if (service != NULL) {
        Characteristic *characteristic = binc_service_get_characteristic(service, characteristic_uuid);
        if (characteristic != NULL) {
            return characteristic;
        }
    }

    return NULL;
}

void binc_device_set_read_char_callback(Device *device, OnReadCallback callback) {
    g_assert(device != NULL);
    g_assert(callback != NULL);
    device->on_read_callback = callback;
}

void binc_device_set_write_char_callback(Device *device, OnWriteCallback callback) {
    g_assert(device != NULL);
    g_assert(callback != NULL);
    device->on_write_callback = callback;
}

void binc_device_set_notify_char_callback(Device *device, OnNotifyCallback callback) {
    g_assert(device != NULL);
    g_assert(callback != NULL);
    device->on_notify_callback = callback;
}

void binc_device_set_notify_state_callback(Device *device, OnNotifyingStateChangedCallback callback) {
    g_assert(device != NULL);
    g_assert(callback != NULL);
    device->on_notify_state_callback = callback;
}

ConnectionState binc_device_get_connection_state(const Device *device) {
    g_assert(device != NULL);
    return device->connection_state;
}

const char* binc_device_get_connection_state_name(const Device *device) {
    g_assert(device != NULL);
    return connection_state_names[device->connection_state];
}

const char *binc_device_get_address(const Device *device) {
    g_assert(device != NULL);
    return device->address;
}

void binc_device_set_address(Device *device, const char *address) {
    g_assert(device != NULL);
    g_assert(address != NULL);

    if (device->address != NULL) {
        g_free((char *) device->address);
    }
    device->address = g_strdup(address);
}

const char *binc_device_get_address_type(const Device *device) {
    g_assert(device != NULL);
    return device->address_type;
}

void binc_device_set_address_type(Device *device, const char *address_type) {
    g_assert(device != NULL);
    g_assert(address_type != NULL);

    if (device->address_type != NULL) {
        g_free((char *) device->address_type);
    }
    device->address_type = g_strdup(address_type);
}

const char *binc_device_get_alias(const Device *device) {
    g_assert(device != NULL);
    return device->alias;
}

void binc_device_set_alias(Device *device, const char *alias) {
    g_assert(device != NULL);
    g_assert(alias != NULL);

    if (device->alias != NULL) {
        g_free((char *) device->alias);
    }
    device->alias = g_strdup(alias);
}

const char *binc_device_get_name(const Device *device) {
    g_assert(device != NULL);
    return device->name;
}

void binc_device_set_name(Device *device, const char *name) {
    g_assert(device != NULL);
    g_assert(name != NULL);

    if (device->name != NULL) {
        g_free((char *) device->name);
    }
    device->name = g_strdup(name);
}

const char *binc_device_get_path(const Device *device) {
    g_assert(device != NULL);
    return device->path;
}

void binc_device_set_path(Device *device, const char *path) {
    g_assert(device != NULL);
    g_assert(path != NULL);

    if (device->path != NULL) {
        g_free((char *) device->path);
    }
    device->path = g_strdup(path);
}

gboolean binc_device_get_paired(const Device *device) {
    g_assert(device != NULL);
    return device->paired;
}

void binc_device_set_paired(Device *device, gboolean paired) {
    g_assert(device != NULL);
    device->paired = paired;
}

short binc_device_get_rssi(const Device *device) {
    g_assert(device != NULL);
    return device->rssi;
}

void binc_device_set_rssi(Device *device, short rssi) {
    g_assert(device != NULL);
    device->rssi = rssi;
}

gboolean binc_device_get_trusted(const Device *device) {
    g_assert(device != NULL);
    return device->trusted;
}

void binc_device_set_trusted(Device *device, gboolean trusted) {
    g_assert(device != NULL);
    device->trusted = trusted;
}

short binc_device_get_txpower(const Device *device) {
    g_assert(device != NULL);
    return device->txpower;
}

void binc_device_set_txpower(Device *device, short txpower) {
    g_assert(device != NULL);
    device->txpower = txpower;
}

GList *binc_device_get_uuids(const Device *device) {
    g_assert(device != NULL);
    return device->uuids;
}

void binc_device_set_uuids(Device *device, GList *uuids) {
    g_assert(device != NULL);

    binc_device_free_uuids(device);
    device->uuids = uuids;
}

GHashTable *binc_device_get_manufacturer_data(const Device *device) {
    g_assert(device != NULL);
    return device->manufacturer_data;
}

void binc_device_set_manufacturer_data(Device *device, GHashTable *manufacturer_data) {
    g_assert(device != NULL);

    binc_device_free_manufacturer_data(device);
    device->manufacturer_data = manufacturer_data;
}

GHashTable *binc_device_get_service_data(const Device *device) {
    g_assert(device != NULL);
    return device->service_data;
}

void binc_device_set_service_data(Device *device, GHashTable *service_data) {
    g_assert(device != NULL);

    binc_device_free_service_data(device);
    device->service_data = service_data;
}

GDBusConnection *binc_device_get_dbus_connection(Device *device) {
    g_assert(device != NULL);
    return device->connection;
}

BondingState binc_device_get_bonding_state(const Device *device) {
    g_assert(device != NULL);
    return device->bondingState;
}

void binc_device_set_bonding_state(Device *device, BondingState bonding_state) {
    g_assert(device != NULL);
    BondingState old_state = device->bondingState;
    device->bondingState = bonding_state;
    if (device->bonding_state_callback != NULL) {
        device->bonding_state_callback(device, device->bondingState, old_state, NULL);
    }
}

Adapter *binc_device_get_adapter(const Device *device) {
    g_assert(device != NULL);
    return device->adapter;
}