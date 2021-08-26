//
// Created by martijn on 26/8/21.
//

#include "adapter.h"

int adapter_call_method(GDBusConnection *connection, const char *adapterPath, const char *method, GVariant *param) {
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(connection,
                                         "org.bluez",
                                         adapterPath,
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

/*
 * Adapter: StartDiscovery
 */
int adapter_start_discovery(GDBusConnection *connection, const char *adapterPath) {
    return adapter_call_method(connection, adapterPath, "StartDiscovery", NULL);
}

/*
 * Adapter: StopDiscovery
 */
int adapter_stop_discovery(GDBusConnection *connection, const char *adapterPath) {
    return adapter_call_method(connection, adapterPath, "StopDiscovery", NULL);
}

int set_discovery_filter(GDBusConnection *connection, const char *adapterPath, short rssi_threshold)
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
    rc = adapter_call_method(connection, adapterPath, "SetDiscoveryFilter", g_variant_new_tuple(&device_dict, 1));
    if(rc) {
        g_print("Not able to set discovery filter\n");
        return 1;
    }

    return 0;
}

int adapter_set_property(GDBusConnection *connection, const char *adapterPath, const char *prop, GVariant *value) {
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(connection,
                                         "org.bluez",
                                         adapterPath,
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

int adapter_power_on(GDBusConnection *connection, const char *adapterPath) {
    return adapter_set_property(connection, adapterPath, "Powered", g_variant_new("b", TRUE));
}

int adapter_power_off(GDBusConnection *connection, const char *adapterPath) {
    return adapter_set_property(connection, adapterPath, "Powered", g_variant_new("b", FALSE));
}