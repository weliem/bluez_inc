//
// Created by martijn on 26/8/21.
//

#include "central_manager.h"
#include "adapter.h"
#include "logger.h"

#define BT_ADDRESS_STRING_SIZE 18
const char* TAG = "CentralManager";


void register_scan_result_callback(CentralManager *centralManager, CentralManagerScanResultCallback callback) {
    if (centralManager == NULL) {
        log_info(TAG, "central manager is null");
    }
    if (callback == NULL) {
        log_info(TAG, "scanResultCallback is null");
    }

    centralManager->scanResultCallback = callback;
}

int scan_for_peripherals(CentralManager* centralManager) {
    log_debug(TAG, "starting scan");
    set_discovery_filter(centralManager->dbusConnection, centralManager->adapterPath, -100);
    return adapter_start_discovery(centralManager->dbusConnection, centralManager->adapterPath);
}

int stop_scanning(CentralManager *centralManager) {
    log_debug(TAG, "stopping scan");
    return adapter_stop_discovery(centralManager->dbusConnection, centralManager->adapterPath);
}

static void bluez_signal_adapter_changed(GDBusConnection *conn,
                                         const gchar *sender,
                                         const gchar *path,
                                         const gchar *interface,
                                         const gchar *signal,
                                         GVariant *params,
                                         void *userdata) {
    (void) conn;
    (void) sender;
    (void) path;
    (void) interface;
    (void) userdata;

    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    const char *iface;
    const char *key;
    GVariant *value = NULL;
    const gchar *signature = g_variant_get_type_string(params);

    if (g_strcmp0(signature, "(sa{sv}as)") != 0) {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        goto done;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while (g_variant_iter_next(properties, "{&sv}", &key, &value)) {
        if (!g_strcmp0(key, "Powered")) {
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                g_print("Invalid argument type for %s: %s != %s", key,
                        g_variant_get_type_string(value), "b");
                goto done;
            }
            g_print("Adapter is Powered \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
        }
        if (!g_strcmp0(key, "Discovering")) {
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                g_print("Invalid argument type for %s: %s != %s", key,
                        g_variant_get_type_string(value), "b");
                goto done;
            }
            g_print("Adapter scan \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
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
    (void) sig;
    (void) sender_name;
    (void) object_path;
    (void) interface;
    (void) signal_name;

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
            g_main_loop_quit((GMainLoop *) user_data);
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

GList* variant_array_to_list(GVariant *value) {
    const gchar *type = g_variant_get_type_string(value);
    if (g_strcmp0(type, "as")) return NULL;

    GList *list = NULL;
    gchar *data;
    GVariantIter i;
    g_variant_iter_init(&i, value);
    while (g_variant_iter_next(&i, "s", &data)) {
        g_print("\t\t%s\n", data);
        list = g_list_append(list, data);
    }
    return list;
}

static void bluez_device_appeared(GDBusConnection *sig,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data) {
    (void) sig;
    (void) sender_name;
    (void) object_path;
    (void) interface;
    (void) signal_name;
    (void) user_data;

    GVariantIter *interfaces;
    const char *object;
    const gchar *interface_name;
    GVariant *properties;

    g_variant_get(parameters, "(&oa{sa{sv}})", &object, &interfaces);
    while (g_variant_iter_next(interfaces, "{&s@a{sv}}", &interface_name, &properties)) {
        if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
            g_print("[ %s ]\n", object);

            ScanResult *x = malloc(sizeof(ScanResult));
            init_scan_result(x);
            x->path = object;
            x->interface = interface_name;
            const gchar *property_name;
            GVariantIter i;
            GVariant *prop_val;
            g_variant_iter_init(&i, properties);
            while (g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val)) {
                bluez_property_value(property_name, prop_val);
                if (g_strcmp0(property_name, "Address") == 0) {
                    x->address = g_variant_get_string(prop_val, NULL);
                } else if (g_strcmp0(property_name, "Alias") == 0) {
                    x->alias = g_variant_get_string(prop_val, NULL);
                } else if (g_strcmp0(property_name, "Adapter") == 0) {
                    x->adapter_path = g_variant_get_string(prop_val, NULL);
                } else if (g_strcmp0(property_name, "Name") == 0) {
                    x->name = g_variant_get_string(prop_val, NULL);
                } else if (g_strcmp0(property_name, "RSSI") == 0) {
                    x->rssi = g_variant_get_int16(prop_val);
                } else if (g_strcmp0(property_name, "UUIDs") == 0) {
                    x->uuids = variant_array_to_list(prop_val);
                }
            }

            // Deliver ScanResult to registered callback
            if (user_data != NULL) {
                CentralManager *centralManager = (CentralManager *) user_data;
                if (centralManager->scanResultCallback != NULL) {
                    centralManager->scanResultCallback(x);
                }
            }
            g_variant_unref(prop_val);
        }
        g_variant_unref(properties);
    }
}


void setup_signal_subscribers(CentralManager *centralManager) {
    centralManager->prop_changed = g_dbus_connection_signal_subscribe(centralManager->dbusConnection,
                                                      "org.bluez",
                                                      "org.freedesktop.DBus.Properties",
                                                      "PropertiesChanged",
                                                      NULL,
                                                      "org.bluez.Adapter1",
                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                      bluez_signal_adapter_changed,
                                                      centralManager,
                                                      NULL);

    centralManager->iface_added = g_dbus_connection_signal_subscribe(centralManager->dbusConnection,
                                                     "org.bluez",
                                                     "org.freedesktop.DBus.ObjectManager",
                                                     "InterfacesAdded",
                                                     NULL,
                                                     NULL,
                                                     G_DBUS_SIGNAL_FLAGS_NONE,
                                                     bluez_device_appeared,
                                                     centralManager,
                                                     NULL);

    centralManager->iface_removed = g_dbus_connection_signal_subscribe(centralManager->dbusConnection,
                                                       "org.bluez",
                                                       "org.freedesktop.DBus.ObjectManager",
                                                       "InterfacesRemoved",
                                                       NULL,
                                                       NULL,
                                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                                       bluez_device_disappeared,
                                                       centralManager,
                                                       NULL);
}

void remove_signal_subscribers(CentralManager* centralManager) {
    g_dbus_connection_signal_unsubscribe(centralManager->dbusConnection, centralManager->prop_changed);
    g_dbus_connection_signal_unsubscribe(centralManager->dbusConnection, centralManager->iface_added);
    g_dbus_connection_signal_unsubscribe(centralManager->dbusConnection, centralManager->iface_removed);
}

CentralManager* create_central_manager() {
    log_debug(TAG, "creating central manager");
    CentralManager* centralManager = g_new(CentralManager, 1);
    centralManager->dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    centralManager->adapterPath = "/org/bluez/hci0";
    centralManager->scanResultCallback = NULL;
    setup_signal_subscribers(centralManager);
    return centralManager;
}

void close_central_manager(CentralManager *centralManager) {
    log_debug(TAG, "closing central manager");
    remove_signal_subscribers(centralManager);
    g_object_unref(centralManager->dbusConnection);
    g_free(centralManager);
}