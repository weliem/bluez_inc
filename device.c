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
        { "DISCONNECTED",
          "CONNECTED",
          "CONNECTING",
          "DISCONNECTING"
        };

void binc_init_device(Device *device) {
    device->adapter_path = NULL;
    device->address = NULL;
    device->address_type = NULL;
    device->alias = NULL;
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

Device* binc_create_device(const char* path, GDBusConnection *connection) {
    Device *x = g_new(Device, 1);
    binc_init_device(x);
    x->path = g_strdup(path);
    x->connection = connection;
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
            g_string_append_printf(service_data, "%s -> %s, ", (char*) key, byteArrayString->str);
        }
        g_string_truncate(service_data, service_data->len - 2);
    }
    g_string_append(service_data, "]");

    return g_strdup_printf(
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
}

/**
 * Synchronous method call to a adapter on DBUS
 *
 * @param adapter the adapter to use
 * @param method the method to call
 * @param param parameters for the method (can be NULL)
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int device_call_method(const Device *device, const char *method, GVariant *param) {
    g_assert(device != NULL);
    g_assert(method != NULL);

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(device->connection,
                                                   "org.bluez",
                                                   device->path,
                                                   "org.bluez.Device1",
                                                   method,
                                                   param,
                                                   NULL,
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);
    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", method, error->code, error->message);
        g_clear_error(&error);
        return EXIT_FAILURE;
    }

    if (result != NULL) {
        g_variant_unref(result);
    }
    return EXIT_SUCCESS;
}

void binc_collect_gatt_tree(Device *device) {
    g_assert(device != NULL);

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(device->connection,
                                                   "org.bluez",
                                                   "/",
                                                   "org.freedesktop.DBus.ObjectManager",
                                                   "GetManagedObjects",
                                                   NULL,
                                                   G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);

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

                        Characteristic *characteristic = binc_characteristic_create(device->connection, object_path);

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
                            }
                        }
                        g_variant_unref(prop_val);

                        // Get service and link the characteristic to the service
                        Service *service = g_hash_table_lookup(device->services, characteristic->service_path);
                        if (service != NULL) {
                            characteristic->service_uuid = g_strdup(service->uuid);
                            g_hash_table_insert(device->characteristics, g_strdup(object_path), characteristic);

                            log_debug(TAG, binc_characteristic_to_string(characteristic));
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
}

void device_changed(GDBusConnection *conn,
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
            if (device->services_resolved_callback != NULL && device->services_resolved == TRUE) {
                binc_collect_gatt_tree(device);
                device->services_resolved_callback(device);
            }
        }
    }

    done:
    if (properties != NULL)
        g_variant_iter_free(properties);
    if (value != NULL)
        g_variant_unref(value);
}



int binc_device_connect(Device *device) {
    g_assert(device != NULL);
    g_assert(device->path != NULL);
    if (device->connection_state != DISCONNECTED) return EXIT_FAILURE;

    log_debug(TAG, "Connecting to '%s' (%s)", device->name, device->address);

    device->connection_state = CONNECTING;
    device->device_prop_changed = g_dbus_connection_signal_subscribe(device->connection,
                                                                      "org.bluez",
                                                                      "org.freedesktop.DBus.Properties",
                                                                      "PropertiesChanged",
                                                                      NULL,
                                                                      "org.bluez.Device1",
                                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                                      device_changed,
                                                                      device,
                                                                      NULL);

    device_call_method(device, "Connect", NULL);
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

Characteristic* binc_device_get_characteristic(Device *device, const char* service_uuid, const char* characteristic_uuid) {
    Characteristic *result = NULL;

    if (device->characteristics != NULL && g_hash_table_size(device->characteristics) > 0) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, device->characteristics);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            Characteristic *characteristic = (Characteristic*) value;
            if (g_strcmp0(characteristic->service_uuid, service_uuid) == 0 && g_strcmp0(characteristic->uuid, characteristic_uuid) == 0) {
                return characteristic;
            }
        }
    }

    return result;
}