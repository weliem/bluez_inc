//
// Created by martijn on 26/8/21.
//
// https://review.tizen.org/git/?p=platform/core/connectivity/bluetooth-frwk.git;a=blob;f=bt-api/bt-gatt-client.c;h=b51dbc5e40ce4b391dac6488ffd69f1a21e0e60b;hb=HEAD
//

#include <stdint-gcc.h>
#include "adapter.h"
#include "logger.h"
#include "utility.h"

#define BT_ADDRESS_STRING_SIZE 18
#define TAG "Adapter"

/**
 * Array of all found adapters
 */
GPtrArray *binc_adapters = NULL;

/**
 * Global dbusconnection used in all adapters.
 */
GDBusConnection *binc_dbus_connection = NULL;


void init_adapter(Adapter *adapter) {
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

/**
 * Synchronous method call to a adapter on DBUS
 *
 * @param adapter the adapter to use
 * @param method the method to call
 * @param param parameters for the method (can be NULL)
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int adapter_call_method(const Adapter *adapter, const char *method, GVariant *param) {
    g_assert(adapter != NULL);
    g_assert(method != NULL);

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(adapter->connection,
                                                   "org.bluez",
                                                   adapter->path,
                                                   "org.bluez.Adapter1",
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

    g_variant_unref(result);
    return EXIT_SUCCESS;
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
    const char *key;
    GVariant *value = NULL;

    Adapter *adapter = (Adapter *) userdata;
    g_assert(adapter != NULL);

    const gchar *signature = g_variant_get_type_string(params);
    if (g_strcmp0(signature, "(sa{sv}as)") != 0) {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        goto done;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while (g_variant_iter_next(properties, "{&sv}", &key, &value)) {
        if (!g_strcmp0(key, "Powered")) {
            adapter->powered = g_variant_get_boolean(value);
            if (adapter->poweredStateCallback != NULL) {
                adapter->poweredStateCallback(adapter);
            }
        }
        if (!g_strcmp0(key, "Discovering")) {
            adapter->discovery_state = g_variant_get_boolean(value);
            if (adapter->discoveryStateCallback != NULL) {
                adapter->discoveryStateCallback(adapter);
            }
        }
    }

    done:
    if (properties != NULL)
        g_variant_iter_free(properties);
    if (value != NULL)
        g_variant_unref(value);
}

static void bluez_device_disappeared(GDBusConnection *sig,
                                     const gchar *sender_name,
                                     const gchar *object_path,
                                     const gchar *interface,
                                     const gchar *signal_name,
                                     GVariant *parameters,
                                     gpointer user_data) {

    GVariantIter *interfaces;
    const char *object;
    const gchar *interface_name;
    char address[BT_ADDRESS_STRING_SIZE] = {'\0'};

    g_variant_get(parameters, "(&oas)", &object, &interfaces);
    while (g_variant_iter_next(interfaces, "s", &interface_name)) {
        if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
            int i;
            char *tmp = g_strstr_len(object, -1, "dev_") + 4;

            for (i = 0; *tmp != '\0'; i++, tmp++) {
                if (*tmp == '_') {
                    address[i] = ':';
                    continue;
                }
                address[i] = *tmp;
            }
            g_print("\nDevice %s removed\n", address);
        }
    }
}

static void bluez_property_value(const gchar *key, GVariant *value) {
    const gchar *type = g_variant_get_type_string(value);

    g_print("\t%s : ", key);
    switch (*type) {
        case 'o':
        case 's':
            g_print("%s\n", g_variant_get_string(value, NULL));
            break;
        case 'b':
            g_print("%d\n", g_variant_get_boolean(value));
            break;
        case 'u':
            g_print("%d\n", g_variant_get_uint32(value));
            break;
        case 'n':
            g_print("%d\n", g_variant_get_int16(value));
        case 'a':
            /* TODO Handling only 'as', but not array of dicts */
            if (g_strcmp0(type, "as"))
                break;
            g_print("\n");
            const gchar *uuid;
            GVariantIter i;
            g_variant_iter_init(&i, value);
            while (g_variant_iter_next(&i, "s", &uuid))
                g_print("\t\t%s\n", uuid);
            break;
        default:
            g_print("Other: %s\n", type);
            break;
    }
}


void device_update_property(Device *device, const char *property_name, GVariant *prop_val) {
    const gchar *signature = g_variant_get_type_string(prop_val);
    if (g_strcmp0(property_name, "Address") == 0) {
        device->address = g_variant_get_string(prop_val, NULL);
    } else if (g_strcmp0(property_name, "AddressType") == 0) {
        device->address_type = g_variant_get_string(prop_val, NULL);
    } else if (g_strcmp0(property_name, "Alias") == 0) {
        device->alias = g_variant_get_string(prop_val, NULL);
    } else if (g_strcmp0(property_name, "Adapter") == 0) {
        device->adapter_path = g_variant_get_string(prop_val, NULL);
    } else if (g_strcmp0(property_name, "Name") == 0) {
        device->name = g_variant_get_string(prop_val, NULL);
    } else if (g_strcmp0(property_name, "Paired") == 0) {
        device->paired = g_variant_get_boolean(prop_val);
    } else if (g_strcmp0(property_name, "RSSI") == 0) {
        device->rssi = g_variant_get_int16(prop_val);
    } else if (g_strcmp0(property_name, "TxPower") == 0) {
        device->txpower = g_variant_get_int16(prop_val);
    } else if (g_strcmp0(property_name, "UUIDs") == 0) {
        device->uuids = g_variant_string_array_to_list(prop_val);
    } else if (g_strcmp0(property_name, "ManufacturerData") == 0) {
        GVariantIter *iter;
        g_variant_get(prop_val, "a{qv}", &iter);

        GVariant *array;
        uint16_t key;
        uint8_t val;
        device->manufacturer_data = g_hash_table_new(g_int_hash, g_int_equal);
        while (g_variant_iter_loop(iter, "{qv}", &key, &array)) {
            GByteArray *byteArray = g_byte_array_new();
            GVariantIter it_array;
            g_variant_iter_init(&it_array, array);
            while (g_variant_iter_loop(&it_array, "y", &val)) {
                byteArray = g_byte_array_append(byteArray, &val, 1);
            }
            int *keyCopy = g_new0 (gint, 1);
            *keyCopy = key;
            g_hash_table_insert(device->manufacturer_data, keyCopy, byteArray);

//            GString *bytes = g_byte_array_as_hex(byteArray);
//            log_debug(TAG, "bytes %s", bytes->str);
//            g_string_free(bytes, TRUE);
        }

        g_variant_iter_free(iter);
    } else if (g_strcmp0(property_name, "ServiceData") == 0) {
        GVariantIter *iter;
        g_variant_get(prop_val, "a{sv}", &iter);

        GVariant *array;
        char *key;
        uint8_t val;

        device->service_data = g_hash_table_new(g_str_hash, g_str_equal);
        while (g_variant_iter_loop(iter, "{sv}", &key, &array)) {
            GByteArray *byteArray = g_byte_array_new();
            GVariantIter it_array;
            g_variant_iter_init(&it_array, array);
            while (g_variant_iter_loop(&it_array, "y", &val)) {
                byteArray = g_byte_array_append(byteArray, &val, 1);
            }
            gchar *keyCopy = g_strdup(key);
            g_hash_table_insert(device->service_data, keyCopy, byteArray);

//            GString *bytes = g_byte_array_as_hex(byteArray);
//            log_debug(TAG, "service data %s %s", key, bytes->str);
//            g_string_free(bytes, TRUE);
        }
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
    while (g_variant_iter_next(interfaces, "{&s@a{sv}}", &interface_name, &properties)) {
        if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {

            Device *device = binc_create_device(object, adapter->connection);

            const gchar *property_name;
            GVariantIter i;
            GVariant *prop_val;
            g_variant_iter_init(&i, properties);
            while (g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val)) {
                //               bluez_property_value(property_name, prop_val);
                device_update_property(device, property_name, prop_val);
            }

            // Deliver Device to registered callback
            g_hash_table_insert(adapter->devices_cache, (void *) device->path, device);
            if (adapter->discoveryResultCallback != NULL) {
                adapter->discoveryResultCallback(adapter, device);
            }

            g_variant_unref(prop_val);
        }
        g_variant_unref(properties);
    }
}

Device *device_getall_properties(Adapter *adapter, const char *device_path) {
    Device *scanResult = NULL;
    GVariant *result = g_dbus_connection_call_sync(adapter->connection,
                                                   "org.bluez",
                                                   device_path,
                                                   "org.freedesktop.DBus.Properties",
                                                   "GetAll",
                                                   g_variant_new("(s)", "org.bluez.Device1"),
                                                   G_VARIANT_TYPE("(a{sv})"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   NULL);

    if (result == NULL) {
        g_print("Unable to device properties\n");
        return scanResult;
    }

    scanResult = binc_create_device(device_path, adapter->connection);
    result = g_variant_get_child_value(result, 0);
    const gchar *property_name;
    GVariantIter i;
    GVariant *prop_val;
    g_variant_iter_init(&i, result);
    while (g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val)) {
        //bluez_property_value(property_name, prop_val);
        device_update_property(scanResult, property_name, prop_val);
        g_variant_unref(prop_val);
    }
    return scanResult;
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
    const char *key;
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

    // Look up scanresult for this
    Device *device = g_hash_table_lookup(adapter->devices_cache, path);
    if (device == NULL) {
        device = device_getall_properties(adapter, path);
        g_hash_table_insert(adapter->devices_cache, (void *) device->path, device);
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while (g_variant_iter_next(properties, "{&sv}", &key, &value)) {
        device_update_property(device, key, value);
    }

    if (adapter->discoveryResultCallback != NULL) {
        adapter->discoveryResultCallback(adapter, device);
    }

    done:
    if (properties != NULL)
        g_variant_iter_free(properties);
    if (value != NULL)
        g_variant_unref(value);
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
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->adapter_prop_changed);
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->iface_added);
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->iface_removed);
}

Adapter *create_adapter(GDBusConnection *connection, const char *path) {
    g_assert(connection != NULL);
    g_assert(path != NULL);

    Adapter *adapter = g_new(Adapter, 1);
    init_adapter(adapter);
    adapter->connection = connection;
    adapter->path = path;
    adapter->devices_cache = g_hash_table_new(g_str_hash, g_str_equal);
    setup_signal_subscribers(adapter);
    return adapter;
}

static void bluez_list_controllers(GDBusConnection *con,
                                   GAsyncResult *res,
                                   gpointer data) {
    (void) data;
    GVariant *result = NULL;
    GVariantIter i;
    const gchar *object_path;
    GVariant *ifaces_and_properties;

    result = g_dbus_connection_call_finish(con, res, NULL);

}


void bluez_adapter_getall_property(GDBusConnection *con,
                                   GAsyncResult *res,
                                   gpointer data) {
    GVariant *result = NULL;

    result = g_dbus_connection_call_finish(con, res, NULL);
    if (result == NULL)
        g_print("Unable to get result for GetManagedObjects\n");

    if (result) {
        g_print("\n\nUsing \"GetAll\":\n");
        result = g_variant_get_child_value(result, 0);
        const gchar *property_name;
        GVariantIter i;
        GVariant *prop_val;
        g_variant_iter_init(&i, result);
        while (g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val))
            //bluez_property_value(property_name, prop_val);
            g_variant_unref(prop_val);
    }
}


int adapter_getall_properties(Adapter *adapter) {
    g_dbus_connection_call(adapter->connection,
                           "org.bluez",
                           adapter->path,
                           "org.freedesktop.DBus.Properties",
                           "GetAll",
                           g_variant_new("(s)", "org.bluez.Adapter1"),
                           G_VARIANT_TYPE("(a{sv})"),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) bluez_adapter_getall_property,
                           NULL);
}

GPtrArray *binc_find_adapters() {
    if (binc_dbus_connection == NULL) {
        binc_dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    }

    binc_adapters = g_ptr_array_new();
    log_debug("ADAPTER", "finding adapters");

    GVariant *result = g_dbus_connection_call_sync(binc_dbus_connection,
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
    GVariantIter i;
    const gchar *object_path;
    GVariant *ifaces_and_properties;
    if (result) {
        result = g_variant_get_child_value(result, 0);
        g_variant_iter_init(&i, result);
        while (g_variant_iter_next(&i, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties)) {
            const gchar *interface_name;
            GVariant *properties;
            GVariantIter ii;
            g_variant_iter_init(&ii, ifaces_and_properties);
            while (g_variant_iter_next(&ii, "{&s@a{sv}}", &interface_name, &properties)) {
                if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "adapter")) {
                    Adapter *adapter = create_adapter(binc_dbus_connection, object_path);
                    const gchar *property_name;
                    GVariantIter iii;
                    GVariant *prop_val;
                    g_variant_iter_init(&iii, properties);
                    while (g_variant_iter_next(&iii, "{&sv}", &property_name, &prop_val)) {
                        if (g_strcmp0(property_name, "Address") == 0) {
                            adapter->address = g_variant_get_string(prop_val, NULL);
                        } else if (g_strcmp0(property_name, "Powered") == 0) {
                            adapter->powered = g_variant_get_boolean(prop_val);
                        } else if (g_strcmp0(property_name, "Discovering") == 0) {
                            adapter->discovery_state = g_variant_get_boolean(prop_val);
                        } else if (g_strcmp0(property_name, "Discoverable") == 0) {
                            adapter->discoverable = g_variant_get_boolean(prop_val);
                        }
                    }
                    g_ptr_array_add(binc_adapters, adapter);
                    g_variant_unref(prop_val);
                }
                g_variant_unref(properties);
            }
            g_variant_unref(ifaces_and_properties);
        }
        g_variant_unref(result);
    }

    log_debug(TAG, "found %d adapters", binc_adapters->len);
    return binc_adapters;
}

/*
 * Adapter: StartDiscovery
 */
int binc_adapter_start_discovery(Adapter *adapter) {
    g_assert (adapter != NULL);

    if (adapter->discovery_state == STOPPED) {
        adapter->discovery_state = STARTING;
        return adapter_call_method(adapter, "StartDiscovery", NULL);
    } else {
        return EXIT_SUCCESS;
    }
}

/*
 * Adapter: StopDiscovery
 */
int binc_adapter_stop_discovery(Adapter *adapter) {
    g_assert (adapter != NULL);

    if (adapter->discovery_state == STARTED) {
        adapter->discovery_state = STOPPING;
        return adapter_call_method(adapter, "StopDiscovery", NULL);
    } else {
        return EXIT_SUCCESS;
    }
}

int binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold) {
    int rc;
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(b, "{sv}", "Transport", g_variant_new_string("le"));
    g_variant_builder_add(b, "{sv}", "RSSI", g_variant_new_int16(rssi_threshold));
    g_variant_builder_add(b, "{sv}", "DuplicateData", g_variant_new_boolean(TRUE));

//    GVariantBuilder *u = g_variant_builder_new(G_VARIANT_TYPE_STRING_ARRAY);
//    g_variant_builder_add(u, "s", argv[3]);
//    g_variant_builder_add(b, "{sv}", "UUIDs", g_variant_builder_end(u));

    GVariant *device_dict = g_variant_builder_end(b);
//    g_variant_builder_unref(u);
    g_variant_builder_unref(b);
    rc = adapter_call_method(adapter, "SetDiscoveryFilter", g_variant_new_tuple(&device_dict, 1));
    if (rc) {
        g_print("Not able to set discovery filter\n");
        return 1;
    }

    return 0;
}

int adapter_set_property(Adapter *adapter, const char *prop, GVariant *value) {
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(adapter->connection,
                                         "org.bluez",
                                         adapter->path,
                                         "org.freedesktop.DBus.Properties",
                                         "Set",
                                         g_variant_new("(ssv)", "org.bluez.Adapter1", prop, value),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if (error != NULL)
        return 1;

    g_variant_unref(result);
    return 0;
}

int binc_adapter_power_on(Adapter *adapter) {
    return adapter_set_property(adapter, "Powered", g_variant_new("b", TRUE));
}

int binc_adapter_power_off(Adapter *adapter) {
    return adapter_set_property(adapter, "Powered", g_variant_new("b", FALSE));
}

void binc_adapter_register_discovery_callback(Adapter *adapter, AdapterDiscoveryResultCallback callback) {
    g_assert(adapter != NULL);
    g_assert(callback != NULL);

    adapter->discoveryResultCallback = callback;
}

void binc_adapter_register_discovery_state_callback(Adapter *adapter, AdapterDiscoveryStateChangeCallback callback) {
    g_assert(adapter != NULL);
    g_assert(callback != NULL);

    adapter->discoveryStateCallback = callback;
}

void binc_adapter_register_powered_state_callback(Adapter *adapter, AdapterPoweredStateChangeCallback callback) {
    g_assert(adapter != NULL);
    g_assert(callback != NULL);

    adapter->poweredStateCallback = callback;
}