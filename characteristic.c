//
// Created by martijn on 12/9/21.
//

#include <stdint-gcc.h>
#include "characteristic.h"
#include "logger.h"
#include "utility.h"

#define TAG "Characteristic"

Characteristic *binc_characteristic_create(GDBusConnection *connection, const char *path) {
    Characteristic *characteristic = g_new0(Characteristic, 1);
    characteristic->connection = connection;
    characteristic->path = path;
    characteristic->uuid = NULL;
    characteristic->service_path = NULL;
    characteristic->service_uuid = NULL;
    return characteristic;
}

GByteArray *g_variant_get_byte_array(GVariant *variant) {
    g_assert(variant != NULL);

    const gchar *type = g_variant_get_type_string(variant);
    if (g_strcmp0(type, "(ay)")) return NULL;

    GByteArray *byteArray = g_byte_array_new();
    uint8_t val;
    GVariantIter *iter;

    g_variant_get(variant, "(ay)", &iter);
    while (g_variant_iter_loop(iter, "y", &val)) {
        byteArray = g_byte_array_append(byteArray, &val, 1);
    }

//    GByteArray *byteArrayCopy = g_byte_array_sized_new(byteArray->len);
//    g_byte_array_append(byteArrayCopy,byteArray->data, byteArray->len);
    g_variant_iter_free(iter);
    return byteArray;
}

GByteArray *g_variant_get_byte_array2(GVariant *variant) {
    g_assert(variant != NULL);

    const gchar *type = g_variant_get_type_string(variant);
    if (g_strcmp0(type, "ay")) return NULL;

    GByteArray *byteArray = g_byte_array_new();
    uint8_t val;
    GVariantIter *iter;

    g_variant_get(variant, "ay", &iter);
    while (g_variant_iter_loop(iter, "y", &val)) {
        byteArray = g_byte_array_append(byteArray, &val, 1);
    }

    g_variant_iter_free(iter);
    return byteArray;
}


GByteArray *binc_characteristic_read(Characteristic *characteristic) {
    g_assert(characteristic != NULL);

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(characteristic->connection,
                                                   "org.bluez",
                                                   characteristic->path,
                                                   "org.bluez.GattCharacteristic1",
                                                   "ReadValue",
                                                   g_variant_new("(a{sv})", builder),
                                                   G_VARIANT_TYPE("(ay)"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "ReadValue", error->code, error->message);
        g_clear_error(&error);
        return NULL;
    }

    g_variant_builder_unref(builder);
    if (result != NULL) {
        return g_variant_get_byte_array(result);
    } else return NULL;
}

void binc_signal_characteristic_changed(GDBusConnection *conn,
                                        const gchar *sender,
                                        const gchar *path,
                                        const gchar *interface,
                                        const gchar *signal,
                                        GVariant *params,
                                        void *userdata) {

    Characteristic *characteristic = (Characteristic *) userdata;
    g_assert(characteristic != NULL);

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
        if (g_strcmp0(key, "Notifying") == 0) {
            characteristic->notifying = g_variant_get_boolean(value);
            log_debug(TAG, "notifying %s", characteristic->notifying ? "true" : "false");
            if (characteristic->notify_state_callback != NULL) {
                characteristic->notify_state_callback(characteristic);
            }
            if (characteristic->notifying == FALSE) {
                if (characteristic->notify_signal != 0) {
                    g_dbus_connection_signal_unsubscribe(characteristic->connection, characteristic->notify_signal);
                }
            }
        } else if (g_strcmp0(key, "Value") == 0) {
            GByteArray *byteArray = g_variant_get_byte_array2(value);
            GString *result = g_byte_array_as_hex(byteArray);
            log_debug(TAG, "notification <%s>", result->str);
            g_string_free(result, TRUE);
            if (characteristic->on_notify_callback != NULL) {
                characteristic->on_notify_callback(characteristic, byteArray);
            }
        }
    }

    done:
    if (properties != NULL)
        g_variant_iter_free(properties);
    if (value != NULL)
        g_variant_unref(value);
}

void binc_characteristic_start_notify(Characteristic *characteristic, OnNotifyCallback callback) {
    g_assert(characteristic != NULL);
    g_assert(callback != NULL);

    characteristic->on_notify_callback = callback;

    characteristic->notify_signal = g_dbus_connection_signal_subscribe(characteristic->connection,
                                                                       "org.bluez",
                                                                       "org.freedesktop.DBus.Properties",
                                                                       "PropertiesChanged",
                                                                       NULL,
                                                                       "org.bluez.GattCharacteristic1",
                                                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                                                       binc_signal_characteristic_changed,
                                                                       characteristic,
                                                                       NULL);

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(characteristic->connection,
                                                   "org.bluez",
                                                   characteristic->path,
                                                   "org.bluez.GattCharacteristic1",
                                                   "StartNotify",
                                                   NULL,
                                                   NULL,
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);

    if (result != NULL && error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "ReadValue", error->code, error->message);
        g_clear_error(&error);
        return;
    }
}

void binc_characteristic_stop_notify(Characteristic *characteristic) {
    g_assert(characteristic != NULL);

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(characteristic->connection,
                                                   "org.bluez",
                                                   characteristic->path,
                                                   "org.bluez.GattCharacteristic1",
                                                   "StopNotify",
                                                   NULL,
                                                   NULL,
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);

    if (result != NULL && error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "ReadValue", error->code, error->message);
        g_clear_error(&error);
        return;
    }
}

void binc_characteristic_register_notifying_state_change_callback(Characteristic *characteristic, NotifyingStateChangedCallback callback) {
    g_assert(characteristic != NULL);
    g_assert(callback != NULL);

    characteristic->notify_state_callback = callback;
}