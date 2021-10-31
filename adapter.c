//
// Created by martijn on 26/8/21.
//
// https://review.tizen.org/git/?p=platform/core/connectivity/bluetooth-frwk.git;a=blob;f=bt-api/bt-gatt-client.c;h=b51dbc5e40ce4b391dac6488ffd69f1a21e0e60b;hb=HEAD
//

#include <stdint-gcc.h>
#include "adapter.h"
#include "device.h"
#include "logger.h"
#include "utility.h"

#define TAG "Adapter"

struct binc_adapter {
    // Public stuff
    char *path;
    char *address;
    gboolean powered;
    gboolean discoverable;
    DiscoveryState discovery_state;

    // Internal stuff
    GDBusConnection *connection;
    guint device_prop_changed;
    guint adapter_prop_changed;
    guint iface_added;
    guint iface_removed;

    AdapterDiscoveryResultCallback discoveryResultCallback;
    AdapterDiscoveryStateChangeCallback discoveryStateCallback;
    AdapterPoweredStateChangeCallback poweredStateCallback;

    GHashTable *devices_cache;
};

static void init_adapter(Adapter *adapter) {
    g_assert(adapter != NULL);

    adapter->address = NULL;
    adapter->connection = NULL;
    adapter->discoverable = FALSE;
    adapter->discovery_state = STOPPED;
    adapter->path = NULL;
    adapter->powered = FALSE;
    adapter->discoveryResultCallback = NULL;
    adapter->devices_cache = NULL;
}

static void binc_internal_call_method(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Adapter *adapter = (Adapter *) user_data;
    g_assert(adapter != NULL);

    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_finish(adapter->connection, res, &error);
    if (value != NULL) {
        g_variant_unref(value);
    }

    if (error != NULL) {
        log_debug(TAG, "failed to call adapter method (error %d: %s)", error->code, error->message);
        g_clear_error(&error);
    }
}

void adapter_call_method(Adapter *adapter, const char *method, GVariant *param) {
    g_assert(adapter != NULL);
    g_assert(method != NULL);

    g_dbus_connection_call(adapter->connection,
                           "org.bluez",
                           adapter->path,
                           "org.bluez.Adapter1",
                           method,
                           param,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_call_method,
                           adapter);
}


void bluez_signal_adapter_changed(GDBusConnection *conn,
                                  const gchar *sender,
                                  const gchar *path,
                                  const gchar *interface,
                                  const gchar *signal,
                                  GVariant *params,
                                  void *userdata) {

    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    const char *iface;
    char *key;
    GVariant *value = NULL;

    Adapter *adapter = (Adapter *) userdata;
    g_assert(adapter != NULL);

    const gchar *signature = g_variant_get_type_string(params);
    if (g_strcmp0(signature, "(sa{sv}as)") != 0) {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        goto done;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while (g_variant_iter_loop(properties, "{&sv}", &key, &value)) {
        if (g_str_equal(key, "Powered")) {
            adapter->powered = g_variant_get_boolean(value);
            if (adapter->poweredStateCallback != NULL) {
                adapter->poweredStateCallback(adapter, adapter->powered);
            }
        }
        if (g_str_equal(key, "Discovering")) {
            adapter->discovery_state = g_variant_get_boolean(value);
            if (adapter->discoveryStateCallback != NULL) {
                adapter->discoveryStateCallback(adapter, adapter->discovery_state, NULL);
            }
        }
    }

    done:
    if (properties != NULL)
        g_variant_iter_free(properties);
    if (unknown != NULL)
        g_variant_iter_free(unknown);
}

static void bluez_device_disappeared(GDBusConnection *sig,
                                     const gchar *sender_name,
                                     const gchar *object_path,
                                     const gchar *interface,
                                     const gchar *signal_name,
                                     GVariant *parameters,
                                     gpointer user_data) {

    GVariantIter *interfaces = NULL;
    char *object = NULL;
    gchar *interface_name = NULL;

    Adapter *adapter = (Adapter *) user_data;
    g_assert(adapter != NULL);

    g_variant_get(parameters, "(&oas)", &object, &interfaces);
    while (g_variant_iter_loop(interfaces, "s", &interface_name)) {
        if (g_str_equal(interface_name, "org.bluez.Device1")) {
            log_debug(TAG, "Device %s removed", object);
            gpointer key, value;
            if (g_hash_table_lookup_extended(adapter->devices_cache, object, &key, &value)) {
                g_hash_table_remove(adapter->devices_cache, object);
                g_free(key);
                binc_device_free(value);
            }
        }
    }

    if (interfaces != NULL) {
        g_variant_iter_free(interfaces);
    }
}


void binc_device_update_property(Device *device, const char *property_name, GVariant *property_value) {
    if (g_str_equal(property_name, "Address")) {
        binc_device_set_address(device, g_variant_get_string(property_value, NULL));
    } else if (g_str_equal(property_name, "AddressType")) {
        binc_device_set_address_type(device, g_variant_get_string(property_value, NULL));
    } else if (g_str_equal(property_name, "Alias")) {
        binc_device_set_alias(device, g_variant_get_string(property_value, NULL));
    } else if (g_str_equal(property_name, "Name")) {
        binc_device_set_name(device, g_variant_get_string(property_value, NULL));
    } else if (g_str_equal(property_name, "Paired")) {
        binc_device_set_paired(device, g_variant_get_boolean(property_value));
    } else if (g_str_equal(property_name, "RSSI")) {
        binc_device_set_rssi(device, g_variant_get_int16(property_value));
    } else if (g_str_equal(property_name, "Trusted")) {
        binc_device_set_trusted(device, g_variant_get_boolean(property_value));
    } else if (g_str_equal(property_name, "TxPower")) {
        binc_device_set_txpower(device, g_variant_get_int16(property_value));
    } else if (g_str_equal(property_name, "UUIDs")) {
        binc_device_set_uuids(device, g_variant_string_array_to_list(property_value));
    } else if (g_str_equal(property_name, "ManufacturerData")) {
        GVariantIter *iter;
        g_variant_get(property_value, "a{qv}", &iter);

        GVariant *array;
        uint16_t key;
        uint8_t val;
        GHashTable *manufacturer_data = g_hash_table_new(g_int_hash, g_int_equal);
        while (g_variant_iter_loop(iter, "{qv}", &key, &array)) {
            GByteArray *byteArray = g_byte_array_new();
            GVariantIter it_array;
            g_variant_iter_init(&it_array, array);
            while (g_variant_iter_loop(&it_array, "y", &val)) {
                byteArray = g_byte_array_append(byteArray, &val, 1);
            }
            int *keyCopy = g_new0 (gint, 1);
            *keyCopy = key;
            g_hash_table_insert(manufacturer_data, keyCopy, byteArray);
        }
        binc_device_set_manufacturer_data(device, manufacturer_data);
        g_variant_iter_free(iter);
    } else if (g_str_equal(property_name, "ServiceData")) {
        GVariantIter *iter;
        g_variant_get(property_value, "a{sv}", &iter);

        GVariant *array;
        char *key;
        uint8_t val;

        GHashTable *service_data = g_hash_table_new(g_str_hash, g_str_equal);
        while (g_variant_iter_loop(iter, "{sv}", &key, &array)) {
            GByteArray *byteArray = g_byte_array_new();
            GVariantIter it_array;
            g_variant_iter_init(&it_array, array);
            while (g_variant_iter_loop(&it_array, "y", &val)) {
                byteArray = g_byte_array_append(byteArray, &val, 1);
            }
            gchar *keyCopy = g_strdup(key);
            g_hash_table_insert(service_data, keyCopy, byteArray);
        }
        binc_device_set_service_data(device, service_data);
        g_variant_iter_free(iter);
    }
}

static void bluez_device_appeared(GDBusConnection *sig,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data) {

    GVariantIter *interfaces;
    const char *object;
    const gchar *interface_name;
    GVariant *properties;

    Adapter *adapter = (Adapter *) user_data;
    g_assert(adapter != NULL);

    g_variant_get(parameters, "(&oa{sa{sv}})", &object, &interfaces);
    while (g_variant_iter_loop(interfaces, "{&s@a{sv}}", &interface_name, &properties)) {
        if (g_str_equal(interface_name, "org.bluez.Device1")) {
            Device *device = binc_create_device(object, adapter);

            gchar *property_name;
            GVariantIter iter;
            GVariant *prop_val;
            g_variant_iter_init(&iter, properties);
            while (g_variant_iter_loop(&iter, "{&sv}", &property_name, &prop_val)) {
                binc_device_update_property(device, property_name, prop_val);
            }

            // Update cache and deliver Device to registered callback
            g_hash_table_insert(adapter->devices_cache, g_strdup(binc_device_get_path(device)), device);
            if (adapter->discoveryResultCallback != NULL) {
                adapter->discoveryResultCallback(adapter, device);
            }
        }
    }

    if (interfaces != NULL) {
        g_variant_iter_free(interfaces);
    }
}

static void binc_internal_device_properties_callback(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Device *device = (Device *) user_data;
    g_assert(device != NULL);

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_finish(binc_device_get_dbus_connection(device), res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "StartDiscovery", error->code, error->message);
        g_clear_error(&error);
    }

    if (result != NULL) {
        result = g_variant_get_child_value(result, 0);
        const gchar *property_name;
        GVariantIter i;
        GVariant *property_value;
        g_variant_iter_init(&i, result);
        while (g_variant_iter_loop(&i, "{&sv}", &property_name, &property_value)) {
            binc_device_update_property(device, property_name, property_value);
        }
        g_variant_unref(result);

        Adapter *adapter = binc_device_get_adapter(device);
        if (adapter != NULL && adapter->discoveryResultCallback != NULL) {
            adapter->discoveryResultCallback(adapter, device);
        }
    }
}

static void device_getall_properties(Adapter *adapter, Device *device) {
    g_dbus_connection_call(adapter->connection,
                           "org.bluez",
                           binc_device_get_path(device),
                           "org.freedesktop.DBus.Properties",
                           "GetAll",
                           g_variant_new("(s)", "org.bluez.Device1"),
                           G_VARIANT_TYPE("(a{sv})"),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_device_properties_callback,
                           device);
}

void bluez_signal_device_changed(GDBusConnection *conn,
                                 const gchar *sender,
                                 const gchar *path,
                                 const gchar *interface,
                                 const gchar *signal,
                                 GVariant *params,
                                 void *userdata) {

    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    const char *iface;
    char *key;
    GVariant *value = NULL;
    const gchar *signature = g_variant_get_type_string(params);

    Adapter *adapter = (Adapter *) userdata;
    g_assert(adapter != NULL);

    // If we are not scanning we're bailing out
    if (adapter->discovery_state != STARTED) return;

    if (g_strcmp0(signature, "(sa{sv}as)") != 0) {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        goto done;
    }

    // Look up device
    Device *device = g_hash_table_lookup(adapter->devices_cache, path);
    if (device == NULL) {
        device = binc_create_device(path, adapter);
        g_hash_table_insert(adapter->devices_cache, g_strdup(binc_device_get_path(device)), device);
        device_getall_properties(adapter, device);
    } else {
        g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
        while (g_variant_iter_loop(properties, "{&sv}", &key, &value)) {
            binc_device_update_property(device, key, value);
        }

        if (adapter->discoveryResultCallback != NULL) {
            adapter->discoveryResultCallback(adapter, device);
        }
    }

    done:
    if (properties != NULL)
        g_variant_iter_free(properties);
    if (unknown != NULL)
        g_variant_iter_free(unknown);
}

void setup_signal_subscribers(Adapter *adapter) {
    adapter->device_prop_changed = g_dbus_connection_signal_subscribe(adapter->connection,
                                                                      "org.bluez",
                                                                      "org.freedesktop.DBus.Properties",
                                                                      "PropertiesChanged",
                                                                      NULL,
                                                                      "org.bluez.Device1",
                                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                                      bluez_signal_device_changed,
                                                                      adapter,
                                                                      NULL);

    adapter->adapter_prop_changed = g_dbus_connection_signal_subscribe(adapter->connection,
                                                                       "org.bluez",
                                                                       "org.freedesktop.DBus.Properties",
                                                                       "PropertiesChanged",
                                                                       NULL,
                                                                       "org.bluez.Adapter1",
                                                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                                                       bluez_signal_adapter_changed,
                                                                       adapter,
                                                                       NULL);

    adapter->iface_added = g_dbus_connection_signal_subscribe(adapter->connection,
                                                              "org.bluez",
                                                              "org.freedesktop.DBus.ObjectManager",
                                                              "InterfacesAdded",
                                                              NULL,
                                                              NULL,
                                                              G_DBUS_SIGNAL_FLAGS_NONE,
                                                              bluez_device_appeared,
                                                              adapter,
                                                              NULL);

    adapter->iface_removed = g_dbus_connection_signal_subscribe(adapter->connection,
                                                                "org.bluez",
                                                                "org.freedesktop.DBus.ObjectManager",
                                                                "InterfacesRemoved",
                                                                NULL,
                                                                NULL,
                                                                G_DBUS_SIGNAL_FLAGS_NONE,
                                                                bluez_device_disappeared,
                                                                adapter,
                                                                NULL);
}

void remove_signal_subscribers(Adapter *adapter) {
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->device_prop_changed);
    adapter->device_prop_changed = 0;
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->adapter_prop_changed);
    adapter->device_prop_changed = 0;
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->iface_added);
    adapter->iface_added = 0;
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->iface_removed);
    adapter->iface_removed = 0;
}

Adapter *create_adapter(GDBusConnection *connection, const char *path) {
    g_assert(connection != NULL);
    g_assert(path != NULL);

    Adapter *adapter = g_new(Adapter, 1);
    init_adapter(adapter);
    adapter->connection = connection;
    adapter->path = g_strdup(path);
    adapter->devices_cache = g_hash_table_new(g_str_hash, g_str_equal);
    setup_signal_subscribers(adapter);
    return adapter;
}

void binc_adapter_free(Adapter *adapter) {
    g_assert(adapter != NULL);

    remove_signal_subscribers(adapter);

    // Free devices cache
    if (adapter->devices_cache != NULL) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, adapter->devices_cache);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_free((char*)key);
            Device *device = (Device *) value;
            binc_device_free(device);
        }
        g_hash_table_destroy(adapter->devices_cache);
    }
    adapter->devices_cache = NULL;

    g_free(adapter->path);
    adapter->path = NULL;
    g_free(adapter->address);
    adapter->address = NULL;
    g_free(adapter);
}


GPtrArray *binc_find_adapters(GDBusConnection *dbusConnection) {
    g_assert(dbusConnection != NULL);

    GPtrArray *binc_adapters = g_ptr_array_new();
    log_debug(TAG, "finding adapters");

    GVariant *result = g_dbus_connection_call_sync(dbusConnection,
                                                   "org.bluez",
                                                   "/",
                                                   "org.freedesktop.DBus.ObjectManager",
                                                   "GetManagedObjects",
                                                   NULL,
                                                   G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   NULL);

    if (result == NULL)
        g_print("Unable to get result for GetManagedObjects\n");

    /* Parse the result */
    GVariantIter iter;
    const gchar *object_path;
    GVariant *ifaces_and_properties;
    if (result) {
        result = g_variant_get_child_value(result, 0);
        g_variant_iter_init(&iter, result);
        while (g_variant_iter_loop(&iter, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties)) {
            const gchar *interface_name;
            GVariant *properties;
            GVariantIter iter2;
            g_variant_iter_init(&iter2, ifaces_and_properties);
            while (g_variant_iter_loop(&iter2, "{&s@a{sv}}", &interface_name, &properties)) {
                if (g_str_equal(interface_name, "org.bluez.Adapter1")) {
                    Adapter *adapter = create_adapter(dbusConnection, object_path);
                    gchar *property_name;
                    GVariantIter iter3;
                    GVariant *prop_val;
                    g_variant_iter_init(&iter3, properties);
                    while (g_variant_iter_loop(&iter3, "{&sv}", &property_name, &prop_val)) {
                        if (g_strcmp0(property_name, "Address") == 0) {
                            adapter->address = g_strdup(g_variant_get_string(prop_val, NULL));
                        } else if (g_strcmp0(property_name, "Powered") == 0) {
                            adapter->powered = g_variant_get_boolean(prop_val);
                        } else if (g_strcmp0(property_name, "Discovering") == 0) {
                            adapter->discovery_state = g_variant_get_boolean(prop_val);
                        } else if (g_strcmp0(property_name, "Discoverable") == 0) {
                            adapter->discoverable = g_variant_get_boolean(prop_val);
                        }
                    }
                    g_ptr_array_add(binc_adapters, adapter);
                }
            }
        }
        g_variant_unref(result);
    }

    log_debug(TAG, "found %d adapters", binc_adapters->len);
    return binc_adapters;
}

Adapter *binc_get_default_adapter(GDBusConnection *dbusConnection) {
    g_assert(dbusConnection != NULL);
    Adapter *adapter = NULL;
    GPtrArray *adapters = binc_find_adapters(dbusConnection);
    if (adapters->len > 0) {
        // Choose the first one in the array, typically the 'hciX' with the highest X
        adapter = g_ptr_array_index(adapters, 0);

        // Free any other adapters we are not going to use
        for (int i = 1; i < adapters->len; i++) {
            binc_adapter_free(g_ptr_array_index(adapters, i));
        }
        g_ptr_array_free(adapters, TRUE);
    }
    return adapter;
}

static void binc_internal_start_discovery_callback(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Adapter *adapter = (Adapter *) user_data;
    g_assert(adapter != NULL);

    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_finish(adapter->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "StartDiscovery", error->code, error->message);
        adapter->discovery_state = STOPPED;
        if (adapter->discoveryStateCallback != NULL) {
            adapter->discoveryStateCallback(adapter, adapter->discovery_state, error);
        }
        g_clear_error(&error);
    }

    if (value != NULL) {
        g_variant_unref(value);
    }
}

void binc_adapter_start_discovery(Adapter *adapter) {
    g_assert (adapter != NULL);

    if (adapter->discovery_state == STOPPED) {
        adapter->discovery_state = STARTING;
        g_dbus_connection_call(adapter->connection,
                               "org.bluez",
                               adapter->path,
                               "org.bluez.Adapter1",
                               "StartDiscovery",
                               NULL,
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               NULL,
                               (GAsyncReadyCallback) binc_internal_start_discovery_callback,
                               adapter);
    }
}

static void binc_internal_stop_discovery_callback(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Adapter *adapter = (Adapter *) user_data;
    g_assert(adapter != NULL);

    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_finish(adapter->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "StopDiscovery", error->code, error->message);
        if (adapter->discoveryStateCallback != NULL) {
            adapter->discoveryStateCallback(adapter, adapter->discovery_state, error);
        }
        g_clear_error(&error);
    }

    if (value != NULL) {
        g_variant_unref(value);
    }
}

void binc_adapter_stop_discovery(Adapter *adapter) {
    g_assert (adapter != NULL);

    if (adapter->discovery_state == STARTED) {
        adapter->discovery_state = STOPPING;
        g_dbus_connection_call(adapter->connection,
                               "org.bluez",
                               adapter->path,
                               "org.bluez.Adapter1",
                               "StopDiscovery",
                               NULL,
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               NULL,
                               (GAsyncReadyCallback) binc_internal_stop_discovery_callback,
                               adapter);
    }
}

void binc_adapter_remove_device(Adapter *adapter, Device *device) {
    g_assert(device != NULL);
    g_assert (adapter != NULL);

    log_debug(TAG, "removing %s", binc_device_get_name(device));
    adapter_call_method(adapter, "RemoveDevice", g_variant_new("(o)", binc_device_get_path(device)));
}

void binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold, GPtrArray *service_uuids) {
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(b, "{sv}", "Transport", g_variant_new_string("le"));
    g_variant_builder_add(b, "{sv}", "RSSI", g_variant_new_int16(rssi_threshold));
    g_variant_builder_add(b, "{sv}", "DuplicateData", g_variant_new_boolean(TRUE));

    if (service_uuids != NULL && service_uuids->len > 0) {
        GVariantBuilder *u = g_variant_builder_new(G_VARIANT_TYPE_STRING_ARRAY);
        for (int i = 0; i < service_uuids->len; i++) {
            g_variant_builder_add(u, "s", g_ptr_array_index(service_uuids, i));
        }
        g_variant_builder_add(b, "{sv}", "UUIDs", g_variant_builder_end(u));
        g_variant_builder_unref(u);
    }

    GVariant *device_dict = g_variant_builder_end(b);
    g_variant_builder_unref(b);
    adapter_call_method(adapter, "SetDiscoveryFilter", g_variant_new_tuple(&device_dict, 1));
}

static void binc_internal_set_property(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Adapter *adapter = (Adapter *) user_data;
    g_assert(adapter != NULL);

    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_finish(adapter->connection, res, &error);
    if (value != NULL) {
        g_variant_unref(value);
    }

    if (error != NULL) {
        log_debug(TAG, "failed to set adapter property (error %d: %s)", error->code, error->message);
        g_clear_error(&error);
    }
}

void adapter_set_property(Adapter *adapter, const char *prop, GVariant *value) {
    g_dbus_connection_call(adapter->connection,
                           "org.bluez",
                           adapter->path,
                           "org.freedesktop.DBus.Properties",
                           "Set",
                           g_variant_new("(ssv)", "org.bluez.Adapter1", prop, value),
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_set_property,
                           adapter);
}

void binc_adapter_power_on(Adapter *adapter) {
    adapter_set_property(adapter, "Powered", g_variant_new("b", TRUE));
}

void binc_adapter_power_off(Adapter *adapter) {
    adapter_set_property(adapter, "Powered", g_variant_new("b", FALSE));
}

void binc_adapter_set_discovery_callback(Adapter *adapter, AdapterDiscoveryResultCallback callback) {
    g_assert(adapter != NULL);
    g_assert(callback != NULL);

    adapter->discoveryResultCallback = callback;
}

void binc_adapter_set_discovery_state_callback(Adapter *adapter, AdapterDiscoveryStateChangeCallback callback) {
    g_assert(adapter != NULL);
    g_assert(callback != NULL);

    adapter->discoveryStateCallback = callback;
}

void binc_adapter_set_powered_state_callback(Adapter *adapter, AdapterPoweredStateChangeCallback callback) {
    g_assert(adapter != NULL);
    g_assert(callback != NULL);

    adapter->poweredStateCallback = callback;
}

char *binc_adapter_get_path(Adapter *adapter) {
    g_assert(adapter != NULL);
    return adapter->path;
}

DiscoveryState binc_adapter_get_discovery_state(Adapter *adapter) {
    g_assert(adapter != NULL);
    return adapter->discovery_state;
}

gboolean binc_adapter_get_powered_state(Adapter *adapter) {
    g_assert(adapter != NULL);
    return adapter->powered;
}

Device *binc_adapter_get_device_by_path(Adapter *adapter, const char *path) {
    g_assert(adapter != NULL);
    return g_hash_table_lookup(adapter->devices_cache, path);
}

GDBusConnection *binc_adapter_get_dbus_connection(const Adapter *adapter) {
    g_assert(adapter != NULL);
    return adapter->connection;
}