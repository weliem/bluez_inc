//
// Created by martijn on 26/8/21.
//

#include "adapter.h"
#include "logger.h"

GPtrArray* binc_adapters = NULL;

GPtrArray* binc_get_adapters() {
    return binc_adapters;
}

GDBusConnection *binc_dbus_connection = NULL;

void requireNotNull(void* pointer, const char *message) {
    if (pointer == NULL) {
        log_debug("ADAPTER", message);
    }
}

void init_adapter(Adapter *adapter) {
    requireNotNull(adapter, "adapter is null");

    adapter->address = NULL;
    adapter->connection = NULL;
    adapter->discoverable = FALSE;
    adapter->discovering = FALSE;
    adapter->path = NULL;
    adapter->powered = FALSE;
}

Adapter* create_adapter(GDBusConnection *connection, const char* path) {
    Adapter* adapter = g_new(Adapter,1);
    init_adapter(adapter);
    adapter->connection = connection;
    adapter->path = path;
    return adapter;
}

int adapter_call_method(Adapter *adapter, const char *method, GVariant *param) {
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(adapter->connection,
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
    if (error != NULL)
        return 1;

    g_variant_unref(result);
    return 0;
}



static void bluez_list_controllers(GDBusConnection *con,
                                   GAsyncResult *res,
                                   gpointer data)
{
    (void)data;
    GVariant *result = NULL;
    GVariantIter i;
    const gchar *object_path;
    GVariant *ifaces_and_properties;

    result = g_dbus_connection_call_finish(con, res, NULL);
    if(result == NULL)
        g_print("Unable to get result for GetManagedObjects\n");

    /* Parse the result */
    if(result) {
        result = g_variant_get_child_value(result, 0);
        g_variant_iter_init(&i, result);
        while(g_variant_iter_next(&i, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties)) {
            const gchar *interface_name;
            GVariant *properties;
            GVariantIter ii;
            g_variant_iter_init(&ii, ifaces_and_properties);
            while(g_variant_iter_next(&ii, "{&s@a{sv}}", &interface_name, &properties)) {
                if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "adapter")) {
                    Adapter* adapter = create_adapter(con, object_path);
                    const gchar *property_name;
                    GVariantIter iii;
                    GVariant *prop_val;
                    g_variant_iter_init(&iii, properties);
                    while(g_variant_iter_next(&iii, "{&sv}", &property_name, &prop_val)) {
                        if (g_strcmp0(property_name, "Address") == 0) {
                            adapter->address = g_variant_get_string(prop_val, NULL);
                        } else if (g_strcmp0(property_name, "Powered") == 0) {
                            adapter->powered = g_variant_get_boolean(prop_val);
                        } else if (g_strcmp0(property_name, "Discovering") == 0) {
                            adapter->discovering = g_variant_get_boolean(prop_val);
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

        // Deliver results to registered callback
        if (data != NULL) {
            FindAdaptersCallback callback = (FindAdaptersCallback ) data;
            callback(binc_adapters);
        }
    }
}

void bluez_adapter_getall_property(GDBusConnection *con,
                                          GAsyncResult *res,
                                          gpointer data)
{
    GVariant *result = NULL;

    result = g_dbus_connection_call_finish(con, res, NULL);
    if(result == NULL)
        g_print("Unable to get result for GetManagedObjects\n");

    if(result) {
        g_print("\n\nUsing \"GetAll\":\n");
        result = g_variant_get_child_value(result, 0);
        const gchar *property_name;
        GVariantIter i;
        GVariant *prop_val;
        g_variant_iter_init(&i, result);
        while(g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val))
            //bluez_property_value(property_name, prop_val);
        g_variant_unref(prop_val);
    }
}

int adapter_get_properties(Adapter *adapter) {
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

int binc_find_adapters(FindAdaptersCallback callback) {
    if (binc_dbus_connection == NULL) {
        binc_dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    }

    binc_adapters = g_ptr_array_new();
    log_debug("ADAPTER", "finding adapters");

    g_dbus_connection_call(binc_dbus_connection,
                           "org.bluez",
                           "/",
                           "org.freedesktop.DBus.ObjectManager",
                           "GetManagedObjects",
                           NULL,
                           G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback)bluez_list_controllers,
                           callback);
}

/*
 * Adapter: StartDiscovery
 */
int binc_adapter_start_discovery(Adapter *adapter) {
    return adapter_call_method(adapter, "StartDiscovery", NULL);
}

/*
 * Adapter: StopDiscovery
 */
int binc_adapter_stop_discovery(Adapter *adapter) {
    return adapter_call_method(adapter, "StopDiscovery", NULL);
}

int binc_adapter_set_discovery_filter(Adapter *adapter, short rssi_threshold)
{
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
    if(rc) {
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