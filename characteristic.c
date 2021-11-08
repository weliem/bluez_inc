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
#include "characteristic.h"
#include "logger.h"
#include "utility.h"
#include "device.h"

#define TAG "Characteristic"

struct binc_characteristic {
    Device *device;
    GDBusConnection *connection;
    const char *path;
    const char *uuid;
    const char *service_path;
    const char *service_uuid;
    gboolean notifying;
    GList *flags;
    guint properties;

    guint notify_signal;
    OnNotifyingStateChangedCallback notify_state_callback;
    OnReadCallback on_read_callback;
    OnWriteCallback on_write_callback;
    OnNotifyCallback on_notify_callback;
} ;

Characteristic *binc_characteristic_create(Device *device, const char *path) {
    Characteristic *characteristic = g_new0(Characteristic, 1);
    characteristic->device = device;
    characteristic->connection = binc_device_get_dbus_connection(device);
    characteristic->path = g_strdup(path);
    characteristic->uuid = NULL;
    characteristic->service_path = NULL;
    characteristic->service_uuid = NULL;
    characteristic->notifying = FALSE;
    characteristic->flags = NULL;
    characteristic->properties = 0;
    characteristic->notify_signal = 0;
    characteristic->notify_state_callback = NULL;
    characteristic->on_read_callback = NULL;
    characteristic->on_write_callback = NULL;
    characteristic->on_notify_callback = NULL;
    return characteristic;
}

static void binc_characteristic_free_flags(Characteristic *characteristic) {
    if (characteristic->flags != NULL) {
        if (g_list_length(characteristic->flags) > 0) {
            for (GList *iterator = characteristic->flags; iterator; iterator = iterator->next) {
                g_free((char *) iterator->data);
            }
        }
        g_list_free(characteristic->flags);
    }
    characteristic->flags = NULL;
}

void binc_characteristic_free(Characteristic *characteristic) {
    g_assert(characteristic != NULL);

    // Unsubscribe signal
    if (characteristic->notify_signal != 0) {
        g_dbus_connection_signal_unsubscribe(characteristic->connection, characteristic->notify_signal);
        characteristic->notify_signal = 0;
    }

    g_free((char *) characteristic->uuid);
    characteristic->uuid = NULL;
    g_free((char *) characteristic->path);
    characteristic->path = NULL;
    g_free((char *) characteristic->service_path);
    characteristic->service_path = NULL;
    g_free((char *) characteristic->service_uuid);
    characteristic->service_uuid = NULL;

    // Free flags
    binc_characteristic_free_flags(characteristic);

    g_free(characteristic);
}

char *binc_characteristic_to_string(const Characteristic *characteristic) {
    g_assert(characteristic != NULL);

    // Build up flags
    GString *flags = g_string_new("[");
    if (g_list_length(characteristic->flags) > 0) {
        for (GList *iterator = characteristic->flags; iterator; iterator = iterator->next) {
            g_string_append_printf(flags, "%s, ", (char *) iterator->data);
        }
        g_string_truncate(flags, flags->len - 2);
    }
    g_string_append(flags, "]");

    char *result = g_strdup_printf(
            "Characteristic{uuid='%s', flags='%s', properties=%d, service_uuid='%s'}",
            characteristic->uuid,
            flags->str,
            characteristic->properties,
            characteristic->service_uuid);

    g_string_free(flags, TRUE);
    return result;
}

static GByteArray *g_variant_get_byte_array_for_read(GVariant *variant) {
    g_assert(variant != NULL);

    const gchar *type = g_variant_get_type_string(variant);
    if (!g_str_equal(type, "(ay)")) return NULL;

    GByteArray *byteArray = g_byte_array_new();
    uint8_t val;
    GVariantIter *iter;

    g_variant_get(variant, "(ay)", &iter);
    while (g_variant_iter_loop(iter, "y", &val)) {
        byteArray = g_byte_array_append(byteArray, &val, 1);
    }

    g_variant_iter_free(iter);
    return byteArray;
}

static void binc_internal_char_read_callback(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    GByteArray *byteArray = NULL;
    Characteristic *characteristic = (Characteristic *) user_data;
    g_assert(characteristic != NULL);

    GVariant *value = g_dbus_connection_call_finish(characteristic->connection, res, &error);
    if (value != NULL) {
        byteArray = g_variant_get_byte_array_for_read(value);
        g_variant_unref(value);
    }

    if (characteristic->on_read_callback != NULL) {
        characteristic->on_read_callback(characteristic, byteArray, error);
    }

    if (byteArray != NULL) {
        g_byte_array_free(byteArray, TRUE);
    }

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "ReadValue", error->code, error->message);
        g_clear_error(&error);
    }
}

void binc_characteristic_read(Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    g_assert((characteristic->properties & GATT_CHR_PROP_READ) > 0);

    log_debug(TAG, "reading <%s>", characteristic->uuid);

    guint16 offset = 0;
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(builder, "{sv}", "offset", g_variant_new_uint16(offset));

    g_dbus_connection_call(characteristic->connection,
                           "org.bluez",
                           characteristic->path,
                           "org.bluez.GattCharacteristic1",
                           "ReadValue",
                           g_variant_new("(a{sv})", builder),
                           G_VARIANT_TYPE("(ay)"),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_char_read_callback,
                           characteristic);
    g_variant_builder_unref(builder);
}

static void binc_internal_char_write_callback(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    Characteristic *characteristic = (Characteristic *) user_data;
    g_assert(characteristic != NULL);

    GVariant *value = g_dbus_connection_call_finish(characteristic->connection, res, &error);
    if (value != NULL) {
        g_variant_unref(value);
    }

    if (characteristic->on_write_callback != NULL) {
        characteristic->on_write_callback(characteristic, error);
    }

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "WriteValue", error->code, error->message);
        g_clear_error(&error);
    }
}

void binc_characteristic_write(Characteristic *characteristic, GByteArray *byteArray, WriteType writeType) {
    g_assert(characteristic != NULL);
    g_assert(byteArray != NULL);

    if (writeType == WITH_RESPONSE) {
        g_assert((characteristic->properties & GATT_CHR_PROP_WRITE) > 0);
    } else {
        g_assert((characteristic->properties & GATT_CHR_PROP_WRITE_WITHOUT_RESP) > 0);
    }

    GString *byteArrayStr = g_byte_array_as_hex(byteArray);
    log_debug(TAG, "writing <%s> to <%s>", byteArrayStr->str, characteristic->uuid);
    g_string_free(byteArrayStr, TRUE);

    // Convert byte array to variant
    GVariantBuilder *builder1 = g_variant_builder_new(G_VARIANT_TYPE("ay"));
    for (int i = 0; i < byteArray->len; i++) {
        g_variant_builder_add(builder1, "y", byteArray->data[i]);
    }
    GVariant *value = g_variant_new("ay", builder1);

    // Convert options to variant
    guint16 offset = 0;
    const char *writeTypeString = writeType == WITH_RESPONSE ? "request" : "command";
    GVariantBuilder *builder2 = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(builder2, "{sv}", "offset", g_variant_new_uint16(offset));
    g_variant_builder_add(builder2, "{sv}", "type", g_variant_new_string(writeTypeString));
    GVariant *options = g_variant_new("a{sv}", builder2);
    g_variant_builder_unref(builder1);
    g_variant_builder_unref(builder2);

    g_dbus_connection_call(characteristic->connection,
                           "org.bluez",
                           characteristic->path,
                           "org.bluez.GattCharacteristic1",
                           "WriteValue",
                           g_variant_new("(@ay@a{sv})", value, options),
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_char_write_callback,
                           characteristic);
}

static GByteArray *g_variant_get_byte_array_for_notify(GVariant *variant) {
    g_assert(variant != NULL);

    const gchar *type = g_variant_get_type_string(variant);
    if (!g_str_equal(type, "ay")) return NULL;

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

static void binc_signal_characteristic_changed(GDBusConnection *conn,
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
    if (!g_str_equal(signature, "(sa{sv}as)")) {
        log_debug("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        return;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while (g_variant_iter_loop(properties, "{&sv}", &key, &value)) {
        if (g_str_equal(key, "Notifying")) {
            characteristic->notifying = g_variant_get_boolean(value);
            log_debug(TAG, "notifying %s <%s>", characteristic->notifying ? "true" : "false", characteristic->uuid);
            if (characteristic->notify_state_callback != NULL) {
                characteristic->notify_state_callback(characteristic, NULL);
            }
            if (characteristic->notifying == FALSE) {
                if (characteristic->notify_signal != 0) {
                    g_dbus_connection_signal_unsubscribe(characteristic->connection, characteristic->notify_signal);
                    characteristic->notify_signal = 0;
                }
            }
        } else if (g_str_equal(key, "Value")) {
            GByteArray *byteArray = g_variant_get_byte_array_for_notify(value);
            GString *result = g_byte_array_as_hex(byteArray);
            log_debug(TAG, "notification <%s> on <%s>", result->str, characteristic->uuid);
            g_string_free(result, TRUE);
            if (characteristic->on_notify_callback != NULL) {
                characteristic->on_notify_callback(characteristic, byteArray);
            }
            g_byte_array_free(byteArray, TRUE);
        }
    }

    g_variant_iter_free(properties);
    g_variant_iter_free(unknown);
}

static void binc_internal_char_start_notify(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Characteristic *characteristic = (Characteristic *) user_data;
    g_assert(characteristic != NULL);
    g_assert((characteristic->properties & GATT_CHR_PROP_INDICATE) > 0 ||
             (characteristic->properties & GATT_CHR_PROP_NOTIFY) > 0);

    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_finish(characteristic->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "StartNotify", error->code, error->message);
        if (characteristic->notify_state_callback != NULL) {
            characteristic->notify_state_callback(characteristic, error);
        }
        g_clear_error(&error);
    }

    if (value != NULL) {
        g_variant_unref(value);
    }
}

void binc_characteristic_start_notify(Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    g_assert((characteristic->properties & GATT_CHR_PROP_INDICATE) > 0 ||
             (characteristic->properties & GATT_CHR_PROP_NOTIFY) > 0);

    characteristic->notify_signal = g_dbus_connection_signal_subscribe(characteristic->connection,
                                                                       "org.bluez",
                                                                       "org.freedesktop.DBus.Properties",
                                                                       "PropertiesChanged",
                                                                       characteristic->path,
                                                                       "org.bluez.GattCharacteristic1",
                                                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                                                       binc_signal_characteristic_changed,
                                                                       characteristic,
                                                                       NULL);


    g_dbus_connection_call(characteristic->connection,
                           "org.bluez",
                           characteristic->path,
                           "org.bluez.GattCharacteristic1",
                           "StartNotify",
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_char_start_notify,
                           characteristic);
}

static void binc_internal_char_stop_notify(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    Characteristic *characteristic = (Characteristic *) user_data;
    g_assert(characteristic != NULL);

    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_finish(characteristic->connection, res, &error);

    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", "StopNotify", error->code, error->message);
        if (characteristic->notify_state_callback != NULL) {
            characteristic->notify_state_callback(characteristic, error);
        }
        g_clear_error(&error);
    }

    if (value != NULL) {
        g_variant_unref(value);
    }
}

void binc_characteristic_stop_notify(Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    g_assert((characteristic->properties & GATT_CHR_PROP_INDICATE) > 0 ||
             (characteristic->properties & GATT_CHR_PROP_NOTIFY) > 0);

    g_dbus_connection_call(characteristic->connection,
                           "org.bluez",
                           characteristic->path,
                           "org.bluez.GattCharacteristic1",
                           "StopNotify",
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) binc_internal_char_stop_notify,
                           characteristic);
}

void binc_characteristic_set_read_callback(Characteristic *characteristic, OnReadCallback callback) {
    g_assert(characteristic != NULL);
    g_assert(callback != NULL);
    characteristic->on_read_callback = callback;
}

void binc_characteristic_set_write_callback(Characteristic *characteristic, OnWriteCallback callback) {
    g_assert(characteristic != NULL);
    g_assert(callback != NULL);
    characteristic->on_write_callback = callback;
}

void binc_characteristic_set_notify_callback(Characteristic *characteristic, OnNotifyCallback callback) {
    g_assert(characteristic != NULL);
    g_assert(callback != NULL);
    characteristic->on_notify_callback = callback;
}

void binc_characteristic_set_notifying_state_change_callback(Characteristic *characteristic,
                                                             OnNotifyingStateChangedCallback callback) {
    g_assert(characteristic != NULL);
    g_assert(callback != NULL);

    characteristic->notify_state_callback = callback;
}

const char* binc_characteristic_get_uuid(const Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    return characteristic->uuid;
}

void binc_characteristic_set_uuid(Characteristic *characteristic, const char* uuid) {
    g_assert(characteristic != NULL);
    g_assert(uuid != NULL);

    if (characteristic->uuid != NULL) {
        g_free((char*) characteristic->uuid);
    }
    characteristic->uuid = g_strdup(uuid);
}

const char* binc_characteristic_get_service_uuid(const Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    return characteristic->service_uuid;
}

Device* binc_characteristic_get_device(const Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    return characteristic->device;
}

void binc_characteristic_set_service_uuid(Characteristic *characteristic, const char* service_uuid) {
    g_assert(characteristic != NULL);
    g_assert(service_uuid != NULL);

    if (characteristic->service_uuid != NULL) {
        g_free((char*) characteristic->service_uuid);
    }
    characteristic->service_uuid = g_strdup(service_uuid);
}

const char* binc_characteristic_get_service_path(const Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    return characteristic->service_path;
}

void binc_characteristic_set_service_path(Characteristic *characteristic, const char* service_path) {
    g_assert(characteristic != NULL);
    g_assert(service_path != NULL);

    if (characteristic->service_path != NULL) {
        g_free((char*) characteristic->service_path);
    }
    characteristic->service_path = g_strdup(service_path);
}

GList* binc_characteristic_get_flags(const Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    return characteristic->flags;
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

void binc_characteristic_set_flags(Characteristic *characteristic, GList* flags) {
    g_assert(characteristic != NULL);
    g_assert(flags != NULL);

    if (characteristic->flags != NULL) {
        binc_characteristic_free_flags(characteristic);
    }
    characteristic->flags = flags;
    characteristic->properties = binc_characteristic_flags_to_int(flags);
}

guint binc_characteristic_get_properties(const Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    return characteristic->properties;
}

void binc_characteristic_set_properties(Characteristic *characteristic, guint properties) {
    g_assert(characteristic != NULL);
    characteristic->properties = properties;
}

gboolean binc_characteristic_is_notifying(const Characteristic *characteristic) {
    g_assert(characteristic != NULL);
    return characteristic->notifying;
}

gboolean binc_characteristic_supports_write(const Characteristic *characteristic, const WriteType writeType) {
    if (writeType == WITH_RESPONSE) {
        return (characteristic->properties & GATT_CHR_PROP_WRITE) > 0;
    } else {
        return (characteristic->properties & GATT_CHR_PROP_WRITE_WITHOUT_RESP) > 0;
    }
}

gboolean binc_characteristic_supports_read(const Characteristic *characteristic) {
    return (characteristic->properties & GATT_CHR_PROP_READ) > 0;
}

gboolean binc_characteristic_supports_notify(const Characteristic *characteristic) {
    return ((characteristic->properties & GATT_CHR_PROP_INDICATE) > 0 ||
            (characteristic->properties & GATT_CHR_PROP_NOTIFY) > 0);
}

