//
// Created by martijn on 26/8/21.
//

#include <stdint-gcc.h>
#include <gio/gio.h>
#include "logger.h"
#include "device.h"
#include "utility.h"

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
}

Device* binc_create_device(const char* path, GDBusConnection *connection) {
    Device *x = g_new(Device, 1);
    binc_init_device(x);
    x->path = path;
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

    g_variant_unref(result);
    return EXIT_SUCCESS;
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
//
    Device *device = (Device *) userdata;
    g_assert(device != NULL);
//
//    // If we are not scanning we're bailing out
//    if (adapter->discovering == FALSE) return;

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
            if (device->services_resolved_callback != NULL) {
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