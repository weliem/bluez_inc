//
// Created by martijn on 26/8/21.
//

#include <stdint-gcc.h>
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
    ConnectionStateChangedCallback services_resolved_callback;
    GHashTable *services;
    GHashTable *characteristics;

    OnReadCallback on_read_callback;
    OnWriteCallback on_write_callback;
    OnNotifyCallback on_notify_callback;
    OnNotifyingStateChangedCallback on_notify_state_callback;
};

void binc_init_device(Device *device) {
    device->adapter = NULL;
    device->address = NULL;
    device->address_type = NULL;
    device->alias = NULL;
    device->bondingState = NONE;
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
}

void binc_device_free(Device *device) {
    g_assert(device != NULL);

    log_debug(TAG, "freeing %s", device->path);

    // Unsubscribe properties changed if needed
    if (device->device_prop_changed != 0) {
        g_dbus_connection_signal_unsubscribe(device->connection, device->device_prop_changed);
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

    // Free manufacturer data hash table
    binc_device_free_manufacturer_data(device);

    // Free service data hash table
    binc_device_free_service_data(device);

    // Free uuids
    binc_device_free_uuids(device);
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
    GVariantIter i;
    const gchar *object_path;
    GVariant *ifaces_and_properties;
    if (result) {
        device->services = g_hash_table_new(g_str_hash, g_str_equal);
        device->characteristics = g_hash_table_new(g_str_hash, g_str_equal);
        result = g_variant_get_child_value(result, 0);
        g_variant_iter_init(&i, result);
        while (g_variant_iter_next(&i, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties)) {

            if (g_str_has_prefix(object_path, device->path)) {
                const gchar *interface_name;
                GVariant *properties;
                GVariantIter ii;
                g_variant_iter_init(&ii, ifaces_and_properties);
                while (g_variant_iter_next(&ii, "{&s@a{sv}}", &interface_name, &properties)) {
                    if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "service")) {
                        //log_debug(TAG, "%s", object_path);
                        char *uuid = NULL;
                        const gchar *property_name;
                        GVariantIter iii;
                        GVariant *prop_val;
                        g_variant_iter_init(&iii, properties);
                        while (g_variant_iter_next(&iii, "{&sv}", &property_name, &prop_val)) {
                            if (g_strcmp0(property_name, "UUID") == 0) {
                                uuid = g_strdup(g_variant_get_string(prop_val, NULL));
                            }
                        }
                        Service *service = binc_service_create(g_strdup(object_path), uuid);
                        g_hash_table_insert(device->services, g_strdup(object_path), service);
                        g_variant_unref(prop_val);
                    } else if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "characteristic")) {

                        Characteristic *characteristic = binc_characteristic_create(device, object_path);
                        binc_characteristic_set_read_callback(characteristic, &binc_on_characteristic_read);
                        binc_characteristic_set_write_callback(characteristic, &binc_on_characteristic_write);
                        binc_characteristic_set_notify_callback(characteristic, &binc_on_characteristic_notify);
                        binc_characteristic_set_notifying_state_change_callback(characteristic,
                                                                                &binc_on_characteristic_notification_state_changed);

                        const gchar *property_name;
                        GVariantIter iii;
                        GVariant *prop_val;
                        g_variant_iter_init(&iii, properties);
                        while (g_variant_iter_next(&iii, "{&sv}", &property_name, &prop_val)) {
                            if (g_strcmp0(property_name, "UUID") == 0) {
                                binc_characteristic_set_uuid(characteristic, g_variant_get_string(prop_val, NULL));
                            } else if (g_strcmp0(property_name, "Service") == 0) {
                                binc_characteristic_set_service_path(characteristic,
                                                                     g_variant_get_string(prop_val, NULL));
                            } else if (g_strcmp0(property_name, "Flags") == 0) {
                                binc_characteristic_set_flags(characteristic, g_variant_string_array_to_list(prop_val));
                            }
                        }
                        g_variant_unref(prop_val);

                        // Get service and link the characteristic to the service
                        Service *service = g_hash_table_lookup(device->services,
                                                               binc_characteristic_get_service_path(characteristic));
                        if (service != NULL) {
                            binc_characteristic_set_service_uuid(characteristic, service->uuid);
                            g_hash_table_insert(device->characteristics, g_strdup(object_path), characteristic);

                            char *charString = binc_characteristic_to_string(characteristic);
                            log_debug(TAG, charString);
                            g_free(charString);
                        } else {
                            log_debug(TAG, "could not find service %s",
                                      binc_characteristic_get_service_path(characteristic));
                        }
                    }
                    g_variant_unref(properties);
                }
            }
            g_variant_unref(ifaces_and_properties);
        }
        g_variant_unref(result);
    }

    log_debug(TAG, "found %d services", g_hash_table_size(device->services));
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
    while (g_variant_iter_next(properties, "{&sv}", &key, &value)) {
        if (g_strcmp0(key, "Connected") == 0) {
            device->connection_state = g_variant_get_boolean(value);
            log_debug(TAG, "Connection state: %s", connection_state_names[device->connection_state]);
            if (device->connection_state_callback != NULL) {
                device->connection_state_callback(device);
            }
            if (device->connection_state == DISCONNECTED) {
                // Unsubscribe properties changed signal
                g_dbus_connection_signal_unsubscribe(device->connection, device->device_prop_changed);
                device->device_prop_changed = 0;
            }
        } else if (g_strcmp0(key, "ServicesResolved") == 0) {
            device->services_resolved = g_variant_get_boolean(value);
            log_debug(TAG, "ServicesResolved %s", device->services_resolved ? "true" : "false");
            if (device->services_resolved == TRUE && device->bondingState != BONDING) {
                binc_collect_gatt_tree(device);
            }
        } else if (g_strcmp0(key, "Paired") == 0) {
            device->paired = g_variant_get_boolean(value);
            device->bondingState = device->paired ? BONDED : NONE;
            log_debug(TAG, "Paired %s", device->paired ? "true" : "false");

            // If gatt-tree has not been built yet, start building it
            if (device->services == NULL && device->services_resolved) {
                binc_collect_gatt_tree(device);
            }
        }
    }

    done:
    if (properties != NULL)
        g_variant_iter_free(properties);
    if (value != NULL)
        g_variant_unref(value);
}

static void binc_internal_device_connect(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    Device *device = (Device *) user_data;
    g_assert(device != NULL);

    GVariant *value = g_dbus_connection_call_finish(device->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "Connect", error->code, error->message);
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
                                                                         NULL,
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
              device->paired ? "BONDED" : "BONE NONE");

    device->connection_state = CONNECTING;
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
        g_clear_error(&error);
    }

    if (value != NULL) {
        g_variant_unref(value);
    }
}

void binc_device_pair(Device *device) {
    g_assert(device != NULL);
    log_debug(TAG, "pairing device");

    if (device->connection_state == DISCONNECTED) {
        device->connection_state = CONNECTING;
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
        g_clear_error(&error);
    }

    if (value != NULL) {
        g_variant_unref(value);
    }
}

void binc_device_disconnect(Device *device) {
    g_assert(device != NULL);
    g_assert(device->path != NULL);

    // Don't do anything if we are not disconnected
    if (device->connection_state != DISCONNECTED) return;

    log_debug(TAG, "Disconnecting '%s' (%s)", device->name, device->address);

    device->connection_state = DISCONNECTING;
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

void binc_device_register_connection_state_change_callback(Device *device, ConnectionStateChangedCallback callback) {
    g_assert(device != NULL);
    g_assert(callback != NULL);

    device->connection_state_callback = callback;
}

void binc_device_register_services_resolved_callback(Device *device, ConnectionStateChangedCallback callback) {
    g_assert(device != NULL);
    g_assert(callback != NULL);

    device->services_resolved_callback = callback;
}

Characteristic *
binc_device_get_characteristic(Device *device, const char *service_uuid, const char *characteristic_uuid) {
    Characteristic *result = NULL;

    if (device->characteristics != NULL && g_hash_table_size(device->characteristics) > 0) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, device->characteristics);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            Characteristic *characteristic = (Characteristic *) value;
            if (g_strcmp0(binc_characteristic_get_service_uuid(characteristic), service_uuid) == 0 &&
                g_strcmp0(binc_characteristic_get_uuid(characteristic), characteristic_uuid) == 0) {
                return characteristic;
            }
        }
    }

    return result;
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

ConnectionState binc_device_get_connection_state(Device *device) {
    g_assert(device != NULL);
    return device->connection_state;
}

const char *binc_device_get_address(Device *device) {
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

const char *binc_device_get_address_type(Device *device) {
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

const char *binc_device_get_alias(Device *device) {
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

const char *binc_device_get_name(Device *device) {
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

const char *binc_device_get_path(Device *device) {
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

gboolean binc_device_get_paired(Device *device) {
    g_assert(device != NULL);
    return device->paired;
}

void binc_device_set_paired(Device *device, gboolean paired) {
    g_assert(device != NULL);
    device->paired = paired;
}

short binc_device_get_rssi(Device *device) {
    g_assert(device != NULL);
    return device->rssi;
}

void binc_device_set_rssi(Device *device, short rssi) {
    g_assert(device != NULL);
    device->rssi = rssi;
}

gboolean binc_device_get_trusted(Device *device) {
    g_assert(device != NULL);
    return device->trusted;
}

void binc_device_set_trusted(Device *device, gboolean trusted) {
    g_assert(device != NULL);
    device->trusted = trusted;
}

short binc_device_get_txpower(Device *device) {
    g_assert(device != NULL);
    return device->txpower;
}

void binc_device_set_txpower(Device *device, short txpower) {
    g_assert(device != NULL);
    device->txpower = txpower;
}

GList *binc_device_get_uuids(Device *device) {
    g_assert(device != NULL);
    return device->uuids;
}

void binc_device_set_uuids(Device *device, GList *uuids) {
    g_assert(device != NULL);

    binc_device_free_uuids(device);
    device->uuids = uuids;
}

GHashTable *binc_device_get_manufacturer_data(Device *device) {
    g_assert(device != NULL);
    return device->manufacturer_data;
}

void binc_device_set_manufacturer_data(Device *device, GHashTable *manufacturer_data) {
    g_assert(device != NULL);

    binc_device_free_manufacturer_data(device);
    device->manufacturer_data = manufacturer_data;
}

GHashTable *binc_device_get_service_data(Device *device) {
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

BondingState binc_device_get_bonding_state(Device *device) {
    g_assert(device != NULL);
    return device->bondingState;
}

void binc_device_set_bonding_state(Device *device, BondingState bonding_state) {
    g_assert(device != NULL);
    device->bondingState = bonding_state;
}

Adapter *binc_device_get_adapter(Device *device) {
    g_assert(device != NULL);
    return device->adapter;
}