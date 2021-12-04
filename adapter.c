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

#include <stdint-gcc.h>
#include "adapter.h"
#include "device.h"
#include "device_internal.h"
#include "logger.h"
#include "utility.h"

#define TAG "Adapter"
#define BLUEZ_DBUS "org.bluez"
#define INTERFACE_ADAPTER "org.bluez.Adapter1"
#define INTERFACE_DEVICE "org.bluez.Device1"
#define INTERFACE_OBJECT_MANAGER "org.freedesktop.DBus.ObjectManager"

#define METHOD_START_DISCOVERY "StartDiscovery"
#define METHOD_STOP_DISCOVERY "StopDiscovery"
#define METHOD_REMOVE_DEVICE "RemoveDevice"
#define METHOD_SET_DISCOVERY_FILTER "SetDiscoveryFilter"

#define ADAPTER_PROPERTY_POWERED "Powered"
#define ADAPTER_PROPERTY_DISCOVERING "Discovering"
#define ADAPTER_PROPERTY_ADDRESS "Address"
#define ADAPTER_PROPERTY_DISCOVERABLE "Discoverable"

#define DEVICE_PROPERTY_RSSI "RSSI"
#define DEVICE_PROPERTY_UUIDS "UUIDs"
#define DEVICE_PROPERTY_MANUFACTURER_DATA "ManufacturerData"
#define DEVICE_PROPERTY_SERVICE_DATA "ServiceData"

const char *discovery_state_names[] = {
        [STOPPED] = "stopped",
        [STARTED] = "started",
        [STARTING]  = "starting",
        [STOPPING]  = "stopping"
};

struct binc_adapter {
    char *path;
    char *address;
    gboolean powered;
    gboolean discoverable;
    DiscoveryState discovery_state;

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

static void binc_internal_adapter_call_method_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
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

static void binc_internal_adapter_call_method(Adapter *adapter, const char *method, GVariant *parameters) {
    g_assert(adapter != NULL);
    g_assert(method != NULL);

    g_dbus_connection_call(adapter->connection,
                           BLUEZ_DBUS,
                           adapter->path,
                           INTERFACE_ADAPTER,
                           method,
                           parameters,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_adapter_call_method_cb,
                           adapter);
}

static void binc_internal_set_discovery_state(Adapter *adapter, DiscoveryState discovery_state) {
    adapter->discovery_state = discovery_state;
    if (adapter->discoveryStateCallback != NULL) {
        adapter->discoveryStateCallback(adapter, adapter->discovery_state, NULL);
    }
}

static void binc_internal_adapter_changed(GDBusConnection *conn,
                                          const gchar *sender,
                                          const gchar *path,
                                          const gchar *interface,
                                          const gchar *signal,
                                          GVariant *params,
                                          void *userdata) {

    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    char *iface = NULL;
    char *property_name = NULL;
    GVariant *property_value = NULL;

    Adapter *adapter = (Adapter *) userdata;
    g_assert(adapter != NULL);

    g_assert(g_str_equal(g_variant_get_type_string(params), "(sa{sv}as)"));
    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while (g_variant_iter_loop(properties, "{&sv}", &property_name, &property_value)) {
        if (g_str_equal(property_name, ADAPTER_PROPERTY_POWERED)) {
            adapter->powered = g_variant_get_boolean(property_value);
            if (adapter->poweredStateCallback != NULL) {
                adapter->poweredStateCallback(adapter, adapter->powered);
            }
        }
        if (g_str_equal(property_name, ADAPTER_PROPERTY_DISCOVERING)) {
            DiscoveryState discovery_state = g_variant_get_boolean(property_value);
            binc_internal_set_discovery_state(adapter, discovery_state);
        }
    }

    if (properties != NULL)
        g_variant_iter_free(properties);
    if (unknown != NULL)
        g_variant_iter_free(unknown);
}

static void binc_internal_device_disappeared(GDBusConnection *sig,
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
        if (g_str_equal(interface_name, INTERFACE_DEVICE)) {
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


static void binc_internal_device_appeared(GDBusConnection *sig,
                                          const gchar *sender_name,
                                          const gchar *object_path,
                                          const gchar *interface,
                                          const gchar *signal_name,
                                          GVariant *parameters,
                                          gpointer user_data) {

    GVariantIter *interfaces;
    const char *object = NULL;
    const gchar *interface_name = NULL;
    GVariant *properties = NULL;

    Adapter *adapter = (Adapter *) user_data;
    g_assert(adapter != NULL);

    g_variant_get(parameters, "(&oa{sa{sv}})", &object, &interfaces);
    while (g_variant_iter_loop(interfaces, "{&s@a{sv}}", &interface_name, &properties)) {
        if (g_str_equal(interface_name, INTERFACE_DEVICE)) {
            Device *device = binc_create_device(object, adapter);

            gchar *property_name;
            GVariantIter iter;
            GVariant *property_value;
            g_variant_iter_init(&iter, properties);
            while (g_variant_iter_loop(&iter, "{&sv}", &property_name, &property_value)) {
                binc_internal_device_update_property(device, property_name, property_value);
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

static void binc_internal_device_getall_properties_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Device *device = (Device *) user_data;
    g_assert(device != NULL);

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_finish(binc_device_get_dbus_connection(device), res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "GetAll", error->code, error->message);
        g_clear_error(&error);
    }

    if (result != NULL) {
        GVariant *innerResult = g_variant_get_child_value(result, 0);
        const gchar *property_name;
        GVariantIter i;
        GVariant *property_value;
        g_variant_iter_init(&i, innerResult);
        while (g_variant_iter_loop(&i, "{&sv}", &property_name, &property_value)) {
            binc_internal_device_update_property(device, property_name, property_value);
        }
        g_variant_unref(innerResult);
        g_variant_unref(result);

        Adapter *adapter = binc_device_get_adapter(device);
        if (adapter != NULL && adapter->discoveryResultCallback != NULL) {
            adapter->discoveryResultCallback(adapter, device);
        }
    }
}

static void binc_internal_device_getall_properties(Adapter *adapter, Device *device) {
    g_dbus_connection_call(adapter->connection,
                           BLUEZ_DBUS,
                           binc_device_get_path(device),
                           "org.freedesktop.DBus.Properties",
                           "GetAll",
                           g_variant_new("(s)", INTERFACE_DEVICE),
                           G_VARIANT_TYPE("(a{sv})"),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_device_getall_properties_cb,
                           device);
}

static void binc_internal_device_changed(GDBusConnection *conn,
                                         const gchar *sender,
                                         const gchar *path,
                                         const gchar *interface,
                                         const gchar *signal,
                                         GVariant *params,
                                         void *userdata) {

    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    const char *iface = NULL;
    char *key = NULL;
    GVariant *value = NULL;

    Adapter *adapter = (Adapter *) userdata;
    g_assert(adapter != NULL);

    // If we are not scanning we're bailing out
    if (adapter->discovery_state != STARTED) return;

    g_assert(g_str_equal(g_variant_get_type_string(params), "(sa{sv}as)"));

    // Look up device
    Device *device = g_hash_table_lookup(adapter->devices_cache, path);
    if (device == NULL) {
        device = binc_create_device(path, adapter);
        g_hash_table_insert(adapter->devices_cache, g_strdup(binc_device_get_path(device)), device);
        binc_internal_device_getall_properties(adapter, device);
    } else {
        g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
        while (g_variant_iter_loop(properties, "{&sv}", &key, &value)) {
            binc_internal_device_update_property(device, key, value);
            if (g_str_equal(key, DEVICE_PROPERTY_RSSI) ||
                g_str_equal(key, DEVICE_PROPERTY_MANUFACTURER_DATA) ||
                g_str_equal(key, DEVICE_PROPERTY_SERVICE_DATA)) {

                if (binc_device_get_connection_state(device) == DISCONNECTED) {
                    if (adapter->discoveryResultCallback != NULL) {
                        adapter->discoveryResultCallback(adapter, device);
                    }
                }
            }
        }
    }

    if (properties != NULL)
        g_variant_iter_free(properties);
    if (unknown != NULL)
        g_variant_iter_free(unknown);
}

static void setup_signal_subscribers(Adapter *adapter) {
    adapter->device_prop_changed = g_dbus_connection_signal_subscribe(adapter->connection,
                                                                      BLUEZ_DBUS,
                                                                      "org.freedesktop.DBus.Properties",
                                                                      "PropertiesChanged",
                                                                      NULL,
                                                                      INTERFACE_DEVICE,
                                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                                      binc_internal_device_changed,
                                                                      adapter,
                                                                      NULL);

    adapter->adapter_prop_changed = g_dbus_connection_signal_subscribe(adapter->connection,
                                                                       BLUEZ_DBUS,
                                                                       "org.freedesktop.DBus.Properties",
                                                                       "PropertiesChanged",
                                                                       adapter->path,
                                                                       INTERFACE_ADAPTER,
                                                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                                                       binc_internal_adapter_changed,
                                                                       adapter,
                                                                       NULL);

    adapter->iface_added = g_dbus_connection_signal_subscribe(adapter->connection,
                                                              BLUEZ_DBUS,
                                                              INTERFACE_OBJECT_MANAGER,
                                                              "InterfacesAdded",
                                                              NULL,
                                                              NULL,
                                                              G_DBUS_SIGNAL_FLAGS_NONE,
                                                              binc_internal_device_appeared,
                                                              adapter,
                                                              NULL);

    adapter->iface_removed = g_dbus_connection_signal_subscribe(adapter->connection,
                                                                BLUEZ_DBUS,
                                                                INTERFACE_OBJECT_MANAGER,
                                                                "InterfacesRemoved",
                                                                NULL,
                                                                NULL,
                                                                G_DBUS_SIGNAL_FLAGS_NONE,
                                                                binc_internal_device_disappeared,
                                                                adapter,
                                                                NULL);
}

static void init_adapter(Adapter *adapter) {
    g_assert(adapter != NULL);

    adapter->address = NULL;
    adapter->connection = NULL;
    adapter->discoverable = FALSE;
    adapter->discovery_state = STOPPED;
    adapter->path = NULL;
    adapter->powered = FALSE;
    adapter->device_prop_changed = 0;
    adapter->adapter_prop_changed = 0;
    adapter->iface_added = 0;
    adapter->iface_removed = 0;
    adapter->discoveryResultCallback = NULL;
    adapter->discoveryStateCallback = NULL;
    adapter->poweredStateCallback = NULL;
    adapter->devices_cache = NULL;
}

static Adapter *binc_adapter_create(GDBusConnection *connection, const char *path) {
    g_assert(connection != NULL);
    g_assert(path != NULL);

    Adapter *adapter = g_new0(Adapter, 1);
    init_adapter(adapter);
    adapter->connection = connection;
    adapter->path = g_strdup(path);
    adapter->devices_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                   g_free, (GDestroyNotify) binc_device_free);
    setup_signal_subscribers(adapter);
    return adapter;
}

static void remove_signal_subscribers(Adapter *adapter) {
    g_assert(adapter != NULL);

    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->device_prop_changed);
    adapter->device_prop_changed = 0;
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->adapter_prop_changed);
    adapter->device_prop_changed = 0;
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->iface_added);
    adapter->iface_added = 0;
    g_dbus_connection_signal_unsubscribe(adapter->connection, adapter->iface_removed);
    adapter->iface_removed = 0;
}

void binc_adapter_free(Adapter *adapter) {
    g_assert(adapter != NULL);

    remove_signal_subscribers(adapter);

    // Free devices cache
    if (adapter->devices_cache != NULL) {
        g_hash_table_destroy(adapter->devices_cache);
    }
    adapter->devices_cache = NULL;

    g_free(adapter->path);
    adapter->path = NULL;
    g_free(adapter->address);
    adapter->address = NULL;
    g_free(adapter);
}


static GPtrArray *binc_find_adapters(GDBusConnection *dbusConnection) {
    g_assert(dbusConnection != NULL);

    GPtrArray *binc_adapters = g_ptr_array_new();
    log_debug(TAG, "finding adapters");

    GVariant *result = g_dbus_connection_call_sync(dbusConnection,
                                                   BLUEZ_DBUS,
                                                   "/",
                                                   INTERFACE_OBJECT_MANAGER,
                                                   "GetManagedObjects",
                                                   NULL,
                                                   G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   NULL);

    if (result == NULL)
        g_print("Unable to get result for GetManagedObjects\n");

    GVariantIter iter;
    const gchar *object_path;
    GVariant *ifaces_and_properties;
    if (result) {
        GVariant *innerResult = g_variant_get_child_value(result, 0);
        g_variant_iter_init(&iter, innerResult);
        while (g_variant_iter_loop(&iter, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties)) {
            const gchar *interface_name;
            GVariant *properties;
            GVariantIter iter2;
            g_variant_iter_init(&iter2, ifaces_and_properties);
            while (g_variant_iter_loop(&iter2, "{&s@a{sv}}", &interface_name, &properties)) {
                if (g_str_equal(interface_name, INTERFACE_ADAPTER)) {
                    Adapter *adapter = binc_adapter_create(dbusConnection, object_path);
                    gchar *property_name;
                    GVariantIter iter3;
                    GVariant *prop_val;
                    g_variant_iter_init(&iter3, properties);
                    while (g_variant_iter_loop(&iter3, "{&sv}", &property_name, &prop_val)) {
                        if (g_strcmp0(property_name, ADAPTER_PROPERTY_ADDRESS) == 0) {
                            adapter->address = g_strdup(g_variant_get_string(prop_val, NULL));
                        } else if (g_strcmp0(property_name, ADAPTER_PROPERTY_POWERED) == 0) {
                            adapter->powered = g_variant_get_boolean(prop_val);
                        } else if (g_strcmp0(property_name, ADAPTER_PROPERTY_DISCOVERING) == 0) {
                            adapter->discovery_state = g_variant_get_boolean(prop_val);
                        } else if (g_strcmp0(property_name, ADAPTER_PROPERTY_DISCOVERABLE) == 0) {
                            adapter->discoverable = g_variant_get_boolean(prop_val);
                        }
                    }
                    g_ptr_array_add(binc_adapters, adapter);
                }
            }
        }
        g_variant_unref(innerResult);
        g_variant_unref(result);
    }

    log_debug(TAG, "found %d adapter%s", binc_adapters->len, binc_adapters->len > 1 ? "s" : "");
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

static void binc_internal_start_discovery_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Adapter *adapter = (Adapter *) user_data;
    g_assert(adapter != NULL);

    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_finish(adapter->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", METHOD_START_DISCOVERY, error->code, error->message);
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
        binc_internal_set_discovery_state(adapter, STARTING);
        g_dbus_connection_call(adapter->connection,
                               BLUEZ_DBUS,
                               adapter->path,
                               INTERFACE_ADAPTER,
                               METHOD_START_DISCOVERY,
                               NULL,
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               NULL,
                               (GAsyncReadyCallback) binc_internal_start_discovery_cb,
                               adapter);
    }
}

static void binc_internal_stop_discovery_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Adapter *adapter = (Adapter *) user_data;
    g_assert(adapter != NULL);

    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_finish(adapter->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", METHOD_STOP_DISCOVERY, error->code, error->message);
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
        binc_internal_set_discovery_state(adapter, STOPPING);
        g_dbus_connection_call(adapter->connection,
                               BLUEZ_DBUS,
                               adapter->path,
                               INTERFACE_ADAPTER,
                               METHOD_STOP_DISCOVERY,
                               NULL,
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               NULL,
                               (GAsyncReadyCallback) binc_internal_stop_discovery_cb,
                               adapter);
    }
}

void binc_adapter_remove_device(Adapter *adapter, Device *device) {
    g_assert(device != NULL);
    g_assert (adapter != NULL);

    log_debug(TAG, "removing %s (%s)", binc_device_get_name(device), binc_device_get_address(device));
    binc_internal_adapter_call_method(adapter, METHOD_REMOVE_DEVICE,
                                      g_variant_new("(o)", binc_device_get_path(device)));
}

void binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold, GPtrArray *service_uuids) {
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(b, "{sv}", "Transport", g_variant_new_string("le"));
    g_variant_builder_add(b, "{sv}", DEVICE_PROPERTY_RSSI, g_variant_new_int16(rssi_threshold));
    g_variant_builder_add(b, "{sv}", "DuplicateData", g_variant_new_boolean(TRUE));

    if (service_uuids != NULL && service_uuids->len > 0) {
        GVariantBuilder *u = g_variant_builder_new(G_VARIANT_TYPE_STRING_ARRAY);
        for (int i = 0; i < service_uuids->len; i++) {
            char *uuid = g_ptr_array_index(service_uuids, i);
            g_assert(g_uuid_string_is_valid(uuid));
            g_variant_builder_add(u, "s", uuid);
        }
        g_variant_builder_add(b, "{sv}", DEVICE_PROPERTY_UUIDS, g_variant_builder_end(u));
        g_variant_builder_unref(u);
    }

    GVariant *device_dict = g_variant_builder_end(b);
    g_variant_builder_unref(b);
    binc_internal_adapter_call_method(adapter, METHOD_SET_DISCOVERY_FILTER, g_variant_new_tuple(&device_dict, 1));
}

static void binc_internal_set_property_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
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
                           BLUEZ_DBUS,
                           adapter->path,
                           "org.freedesktop.DBus.Properties",
                           "Set",
                           g_variant_new("(ssv)", INTERFACE_ADAPTER, prop, value),
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_set_property_cb,
                           adapter);
}

void binc_adapter_power_on(Adapter *adapter) {
    adapter_set_property(adapter, ADAPTER_PROPERTY_POWERED, g_variant_new("b", TRUE));
}

void binc_adapter_power_off(Adapter *adapter) {
    adapter_set_property(adapter, ADAPTER_PROPERTY_POWERED, g_variant_new("b", FALSE));
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

const char *binc_adapter_get_path(const Adapter *adapter) {
    g_assert(adapter != NULL);
    return adapter->path;
}

DiscoveryState binc_adapter_get_discovery_state(const Adapter *adapter) {
    g_assert(adapter != NULL);
    return adapter->discovery_state;
}

gboolean binc_adapter_get_powered_state(const Adapter *adapter) {
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

const char *binc_adapter_get_discovery_state_name(const Adapter *adapter) {
    g_assert(adapter != NULL);
    return discovery_state_names[adapter->discovery_state];
}