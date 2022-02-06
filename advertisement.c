//
// Created by martijn on 29-01-22.
//

#include <stdint-gcc.h>
#include "advertisement.h"
#include "adapter.h"
#include "logger.h"

static const char *const TAG = "Advertisement";

struct binc_advertisement {
    char *path;
    char *local_name;
    GPtrArray *services;
    GHashTable *manufacturer_data;
    GHashTable *service_data;
    guint registration_id;
};

GVariant *advertisement_get_property(GDBusConnection *connection,
                                     const gchar *sender,
                                     const gchar *object_path,
                                     const gchar *interface_name,
                                     const gchar *property_name,
                                     GError **error,
                                     gpointer user_data) {

//    log_debug(TAG, "GetProperty called");
    GVariant *ret;
    Advertisement *advertisement = user_data;

    ret = NULL;
    if (g_strcmp0(property_name, "Type") == 0) {
        ret = g_variant_new_string("peripheral");
    } else if (g_strcmp0(property_name, "LocalName") == 0) {
        ret = advertisement->local_name ? g_variant_new_string(advertisement->local_name) : NULL;
    } else if (g_strcmp0(property_name, "ServiceUUIDs") == 0) {
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
        for (int i = 0; i < advertisement->services->len; i++) {
            char *service_uuid = g_ptr_array_index(advertisement->services, i);
            g_variant_builder_add(builder, "s", service_uuid);
        }
        ret = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);
    } else if (g_str_equal(property_name, "ManufacturerData")) {
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{qv}"));
        if (advertisement->manufacturer_data != NULL && g_hash_table_size(advertisement->manufacturer_data) > 0) {
            GHashTableIter iter;
            int *key;
            gpointer value;
            g_hash_table_iter_init(&iter, advertisement->manufacturer_data);
            while (g_hash_table_iter_next(&iter, (gpointer) &key, &value)) {
                GByteArray *byteArray = (GByteArray *) value;
                GVariant *byteArrayVariant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, byteArray->data, byteArray->len, sizeof(guint8));
                guint16 manufacturer_id = *key;
                g_variant_builder_add(builder, "{qv}", manufacturer_id, byteArrayVariant);
            }
        }
        ret = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);
    } else if (g_str_equal(property_name, "ServiceData")) {
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        if (advertisement->service_data != NULL && g_hash_table_size(advertisement->service_data) > 0) {
            GHashTableIter iter;
            gpointer key, value;
            g_hash_table_iter_init(&iter, advertisement->service_data);
            while (g_hash_table_iter_next(&iter, &key, &value)) {
                GByteArray *byteArray = (GByteArray *) value;
                GVariant *byteArrayVariant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, byteArray->data, byteArray->len, sizeof(guint8));
                g_variant_builder_add(builder, "{sv}", (char*) key, byteArrayVariant);
            }
        }
        ret = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);
    }
    return ret;
}

static void advertisement_method_call(GDBusConnection *conn,
                                      const gchar *sender,
                                      const gchar *path,
                                      const gchar *interface,
                                      const gchar *method,
                                      GVariant *params,
                                      GDBusMethodInvocation *invocation,
                                      void *userdata) {
    log_debug(TAG, "advertisement method called");
}

static const GDBusInterfaceVTable advertisement_method_table = {
        .method_call = advertisement_method_call,
        .get_property = advertisement_get_property
};

void binc_advertisement_register(Advertisement *advertisement, const Adapter *adapter) {

    static const gchar bluez_advertisement_introspection_xml[] =
            "<node name='/org/bluez/SampleAdvertisement'>"
            "   <interface name='org.bluez.LEAdvertisement1'>"
            "       <method name='Release'>"
            "       </method>"
            "       <property name='Type' type='s' access='read'/>"
            "       <property name='LocalName' type='s' access='read'/>"
            "       <property name='ManufacturerData' type='a{qv}' access='read'/>"
            "       <property name='ServiceData' type='a{sv}' access='read'/>"
            "       <property name='ServiceUUIDs' type='as' access='read'/>"
            "   </interface>"
            "</node>";

    GError *error = NULL;
    GDBusNodeInfo *info = NULL;
    info = g_dbus_node_info_new_for_xml(bluez_advertisement_introspection_xml, &error);
    advertisement->registration_id = g_dbus_connection_register_object(binc_adapter_get_dbus_connection(adapter),
                                                                       advertisement->path,
                                                                       info->interfaces[0],
                                                                       &advertisement_method_table,
                                                                       advertisement, NULL, &error);

    if (error != NULL) {
        log_debug(TAG, "registering advertisement failed");
    }
    g_dbus_node_info_unref(info);
}

static void byte_array_free(GByteArray *byteArray) { g_byte_array_free(byteArray, TRUE); }

Advertisement *binc_advertisement_create() {
    Advertisement *advertisement = g_new0(Advertisement, 1);
    advertisement->path = g_strdup("/org/bluez/bincadvertisement");
    advertisement->manufacturer_data = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, (GDestroyNotify) byte_array_free);
    advertisement->service_data = g_hash_table_new_full(g_str_hash, g_str_equal,g_free, (GDestroyNotify) byte_array_free);
    return advertisement;
}

void binc_advertisement_free(Advertisement *advertisement) {
    if (advertisement->path != NULL) {
        g_free(advertisement->path);
    }
    if (advertisement->local_name != NULL) {
        g_free(advertisement->local_name);
    }
    if (advertisement->services != NULL) {
        g_ptr_array_free(advertisement->services, TRUE);
    }
    if (advertisement->manufacturer_data != NULL) {
        g_hash_table_destroy(advertisement->manufacturer_data);
    }
    if (advertisement->service_data != NULL) {
        g_hash_table_destroy(advertisement->service_data);
    }
    g_free(advertisement);
}

void binc_advertisement_set_local_name(Advertisement *advertisement, const char *local_name) {
    g_assert(advertisement != NULL);

    if (advertisement->local_name != NULL) {
        g_free(advertisement->local_name);
    }
    if (local_name == NULL) {
        advertisement->local_name = NULL;
    } else {
        advertisement->local_name = g_strdup(local_name);
    }
}

const char *binc_advertisement_get_path(Advertisement *advertisement) {
    g_assert(advertisement != NULL);
    return advertisement->path;
}

void binc_advertisement_set_services(Advertisement *advertisement, const GPtrArray *service_uuids) {
    g_assert(advertisement != NULL);
    g_assert(service_uuids != NULL);

    if (advertisement->services != NULL) {
        g_ptr_array_free(advertisement->services, TRUE);
    }
    advertisement->services = g_ptr_array_new_with_free_func(g_free);

    for (int i = 0; i < service_uuids->len; i++) {
        g_ptr_array_add(advertisement->services, g_strdup(g_ptr_array_index(service_uuids, i)));
    }
}

void binc_advertisement_set_manufacturer_data(Advertisement *advertisement, guint16 manufacturer_id, const GByteArray *byteArray) {
    g_assert(advertisement != NULL);
    g_assert(advertisement->manufacturer_data != NULL);

    int man_id = manufacturer_id;
    if (g_hash_table_lookup(advertisement->manufacturer_data, &man_id) != NULL) {
        g_hash_table_remove(advertisement->manufacturer_data, &man_id);
    }

    int *key = g_new0 (int, 1);
    *key = manufacturer_id;

    GByteArray *value = g_byte_array_sized_new(byteArray->len);
    g_byte_array_append(value,byteArray->data, byteArray->len);

    g_hash_table_insert(advertisement->manufacturer_data, key, value);
}

void binc_advertisement_set_service_data(Advertisement *advertisement, const char* service_uuid, const GByteArray *byteArray) {
    g_assert(advertisement != NULL);
    g_assert(advertisement->service_data != NULL);

    if (g_hash_table_lookup(advertisement->service_data, service_uuid) != NULL) {
        g_hash_table_remove(advertisement->service_data, service_uuid);
    }

    GByteArray *value = g_byte_array_sized_new(byteArray->len);
    g_byte_array_append(value,byteArray->data, byteArray->len);

    g_hash_table_insert(advertisement->service_data, g_strdup(service_uuid), value);
}


