//
// Created by martijn on 12/9/21.
//

#include <stdint-gcc.h>
#include "characteristic.h"
#include "logger.h"

#define TAG "Characteristic"

Characteristic *binc_characteristic_create(GDBusConnection *connection, const char* path) {
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

GVariant* binc_characteristic_call_method(const Characteristic *characteristic, const char *method, GVariant *param) {
    g_assert(characteristic != NULL);
    g_assert(method != NULL);

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(characteristic->connection,
                                                   "org.bluez",
                                                   characteristic->path,
                                                   "org.bluez.GattCharacteristic1",
                                                   method,
                                                   param,
                                                   G_VARIANT_TYPE("(ay)"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);
    if (error != NULL) {
        log_debug(TAG, "failed to call '%s' (error %d: %s)", method, error->code, error->message);
        g_clear_error(&error);
        return NULL;
    }

    return result;
}

GByteArray *binc_characteristic_read(Characteristic *characteristic) {
    g_assert(characteristic != NULL);

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    GVariant *result = binc_characteristic_call_method(characteristic, "ReadValue", g_variant_new("(a{sv})", builder));

    g_variant_builder_unref(builder);
    if (result != NULL) {
        return g_variant_get_byte_array(result);
    } else return NULL;
}