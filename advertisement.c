//
// Created by martijn on 29-01-22.
//

#include "advertisement.h"
#include "adapter.h"
#include "logger.h"

static const char *const TAG = "Advertisement";

struct binc_advertisement {
    char *path;
    char *local_name;
    gboolean includeTxPower;
    guint registration_id;
};

GVariant *advertisement_get_property(GDBusConnection *connection,
                                     const gchar *sender,
                                     const gchar *object_path,
                                     const gchar *interface_name,
                                     const gchar *property_name,
                                     GError **error,
                                     gpointer user_data) {

    log_debug(TAG, "GetProperty called");
    GVariant *ret;
    Advertisement *advertisement = user_data;

    ret = NULL;
    if (g_strcmp0(property_name, "Type") == 0) {
        ret = g_variant_new_string("peripheral");
    } else if (g_strcmp0(property_name, "LocalName") == 0) {
        ret = advertisement->local_name ? g_variant_new_string(advertisement->local_name) : NULL;
    } else if (g_strcmp0(property_name, "ServiceUUIDs") == 0) {
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
        g_variant_builder_add(builder, "s", "00001809-0000-1000-8000-00805f9b34fb");
        ret = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);
    } else if (g_strcmp0(property_name, "IncludeTxPower") == 0) {
        ret = g_variant_new_boolean(advertisement->includeTxPower);
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

void binc_advertisement_register(Advertisement *advertisement, Adapter *adapter) {

    static const gchar bluez_advertisement_introspection_xml[] =
            "<node name='/org/bluez/SampleAdvertisement'>"
            "   <interface name='org.bluez.LEAdvertisement1'>"
            "       <method name='Release'>"
            "       </method>"
            "       <property name='Type' type='s' access='read'/>"
            "       <property name='LocalName' type='s' access='read'/>"
            "       <property name='ServiceUUIDs' type='as' access='read'/>"
            "       <property name='IncludeTxPower' type='b' access='read'/>"
            "   </interface>"
            "</node>";

    GError *error = NULL;
    guint id = 0;
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

Advertisement *binc_advertisement_create() {
    Advertisement *advertisement = g_new0(Advertisement, 1);
    advertisement->path = g_strdup("/org/bluez/bincadvertisement");
    return advertisement;
}

void binc_advertisement_free(Advertisement *advertisement) {
    if (advertisement->path != NULL) {
        g_free(advertisement->path);
    }
    if (advertisement->local_name != NULL) {
        g_free(advertisement->local_name);
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

void binc_advertisement_set_include_tx_power(Advertisement *advertisement, gboolean include_tx_power) {
    g_assert(advertisement != NULL);
    advertisement->includeTxPower = include_tx_power;
}

const char *binc_advertisement_get_path(Advertisement *advertisement) {
    g_assert(advertisement != NULL);
    return advertisement->path;
}


