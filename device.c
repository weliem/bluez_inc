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

#define TAG "Device"

char connection_state_names[4][16] =
        {"DISCONNECTED",
         "CONNECTED",
         "CONNECTING",
         "DISCONNECTING"
        };

void binc_init_device(Device *device) {
    device->adapter_path = NULL;
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
}

Device *binc_create_device(const char *path, GDBusConnection *connection) {
    Device *x = g_new0(Device, 1);
    binc_init_device(x);
    x->path = g_strdup(path);
    x->connection = connection;
}

void binc_device_free(Device *device) {
    g_assert(device != NULL);

    log_debug(TAG, "freeing %s", device->path);

    // Free strings
    g_free((char *) device->path);
    g_free((char *) device->adapter_path);
    g_free((char *) device->address_type);
    g_free((char *) device->address);
    g_free((char *) device->alias);
    g_free((char *) device->name);

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

    // Free manufacturer data hash table
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

    // Free uuids
    if (device->uuids != NULL) {
        for (GList *iterator = device->uuids; iterator; iterator = iterator->next) {
            g_free((char *) iterator->data);
        }
        g_list_free(device->uuids);
    }
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
    characteristic->device->on_read_callback(characteristic, byteArray, error);
}

static void binc_on_characteristic_write(Characteristic *characteristic, GError *error) {
    characteristic->device->on_write_callback(characteristic, error);
}

static void binc_on_characteristic_notify(Characteristic *characteristic, GByteArray *byteArray) {
    characteristic->device->on_notify_callback(characteristic, byteArray);
}

static guint binc_characteristic_flags_to_int(GList *flags) {
    guint result = 0;
    if (g_list_length(flags) > 0) {
        for (GList *iterator = flags; iterator; iterator = iterator->next) {
            char *property = (char *) iterator->data;
            if (g_str_equal(property, "broadcast")) {
                result += GATT_CHR_PROP_BROADCAST;
            } else if (g_str_equal(property, "read")) {
                result += GATT_CHR_PROP_READ;
            } else if (g_str_equal(property, "write-without-response")) {
                result += GATT_CHR_PROP_WRITE_WITHOUT_RESP;
            } else if (g_str_equal(property, "write")) {
                result += GATT_CHR_PROP_WRITE;
            } else if (g_str_equal(property, "notify")) {
                result += GATT_CHR_PROP_NOTIFY;
            } else if (g_str_equal(property, "indicate")) {
                result += GATT_CHR_PROP_INDICATE;
            } else if (g_str_equal(property, "authenticated-signed-writes")) {
                result += GATT_CHR_PROP_AUTH;
            }
        }
    }
    return result;
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
                        Service *service = binc_service_create(device->connection, object_path);
                        const gchar *property_name;
                        GVariantIter iii;
                        GVariant *prop_val;
                        g_variant_iter_init(&iii, properties);
                        while (g_variant_iter_next(&iii, "{&sv}", &property_name, &prop_val)) {
                            if (g_strcmp0(property_name, "UUID") == 0) {
                                service->uuid = g_strdup(g_variant_get_string(prop_val, NULL));
                            }
                        }
                        g_hash_table_insert(device->services, g_strdup(object_path), service);
                        g_variant_unref(prop_val);
                    } else if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "characteristic")) {

                        Characteristic *characteristic = binc_characteristic_create(device, object_path);
                        binc_characteristic_set_read_callback(characteristic, &binc_on_characteristic_read);
                        binc_characteristic_set_write_callback(characteristic, &binc_on_characteristic_write);
                        binc_characteristic_set_notify_callback(characteristic, &binc_on_characteristic_notify);

                        const gchar *property_name;
                        GVariantIter iii;
                        GVariant *prop_val;
                        g_variant_iter_init(&iii, properties);
                        while (g_variant_iter_next(&iii, "{&sv}", &property_name, &prop_val)) {
                            if (g_strcmp0(property_name, "UUID") == 0) {
                                characteristic->uuid = g_strdup(g_variant_get_string(prop_val, NULL));
                            } else if (g_strcmp0(property_name, "Service") == 0) {
                                characteristic->service_path = g_strdup(g_variant_get_string(prop_val, NULL));
                            } else if (g_strcmp0(property_name, "Flags") == 0) {
                                characteristic->flags = g_variant_string_array_to_list(prop_val);
                                characteristic->properties = binc_characteristic_flags_to_int(
                                        characteristic->flags);
                            }
                        }
                        g_variant_unref(prop_val);

                        // Get service and link the characteristic to the service
                        Service *service = g_hash_table_lookup(device->services, characteristic->service_path);
                        if (service != NULL) {
                            characteristic->service_uuid = g_strdup(service->uuid);
                            g_hash_table_insert(device->characteristics, g_strdup(object_path), characteristic);

                            char *charString = binc_characteristic_to_string(characteristic);
                            log_debug(TAG, charString);
                            g_free(charString);
                        } else {
                            log_debug(TAG, "could not find service %s", characteristic->service_path);
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
            log_debug(TAG, "Connected %s", connection_state_names[device->connection_state]);
            if (device->connection_state_callback != NULL) {
                device->connection_state_callback(device);
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

void binc_device_connect(Device *device) {
    g_assert(device != NULL);
    g_assert(device->path != NULL);

    // Don't do anything if we are not disconnected
    if (device->connection_state != DISCONNECTED) return;

    log_debug(TAG, "Connecting to '%s' (%s)", device->name, device->address);

    device->connection_state = CONNECTING;
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
            if (g_strcmp0(characteristic->service_uuid, service_uuid) == 0 &&
                g_strcmp0(characteristic->uuid, characteristic_uuid) == 0) {
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
