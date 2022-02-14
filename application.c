//
// Created by martijn on 11-02-22.
//

#include "application.h"
#include "adapter.h"
#include "logger.h"

#define BUFFER_SIZE 255
#define GATT_SERV_INTERFACE "org.bluez.GattService1"

static const char *const TAG = "Application";

static const gchar manager_introspection_xml[] =
        "<node name='/'>"
        "  <interface name='org.freedesktop.DBus.ObjectManager'>"
        "    <method name='GetManagedObjects'>"
        "     <arg type='a{oa{sa{sv}}}' name='object_paths_interfaces_and_properties' direction='out'/>"
        "        </method>"
        "  </interface>"
        "</node>";

static const gchar service_introspection_xml[] =
        "<node name='/'>"
        "  <interface name='org.freedesktop.DBus.Properties'>"
        "    <property type='s' name='UUID' access='read'>"
        "    </property>"
        "        <property type='b' name='primary' access='read'>"
        "        </property>"
        "        <property type='o' name='Device' access='read'>"
        "        </property>"
        "        <property type='ao' name='Characteristics' access='read'>"
        "        </property>"
        "        <property type='s' name='Includes' access='read'>"
        "        </property>"
        "  </interface>"
        "</node>";

struct binc_application {
    char *path;
    guint registration_id;
    GDBusConnection *connection;
    GHashTable *services;
};

Application *binc_create_application() {
    Application *application = g_new0(Application, 1);
    application->path = g_strdup("/org/bluez/bincapplication");
    application->services = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    return application;
}

void binc_application_free(Application *application) {
    g_assert(application != NULL);

    if (application->path != NULL) {
        g_free(application->path);
    }

    if (application->services != NULL) {
        g_hash_table_destroy(application->services);
    }

    g_free(application);
}

void binc_application_add_service(Application *application, const char *service_uuid) {
    g_assert(application != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));

    char buf[BUFFER_SIZE];
    guint8 count = g_hash_table_size(application->services);
    g_snprintf(buf, BUFFER_SIZE, "%s/service%d", application->path, count);
    g_hash_table_insert(application->services, g_strdup(service_uuid), g_strdup(buf));
}


static void bluez_application_method_call(GDBusConnection *conn,
                                          const gchar *sender,
                                          const gchar *path,
                                          const gchar *interface,
                                          const gchar *method,
                                          GVariant *params,
                                          GDBusMethodInvocation *invocation,
                                          void *userdata) {

    Application *application = (Application *) userdata;
    g_assert(application != NULL);

    if (g_strcmp0(method, "GetManagedObjects") == 0) {
        log_debug(TAG, "GetManagedObjects");

        /* Main Builder */
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{oa{sa{sv}}}"));

        // Add services
        if (application->services != NULL && g_hash_table_size(application->services) > 0) {
            GHashTableIter iter;
            gpointer key, value;
            g_hash_table_iter_init(&iter, application->services);
            while (g_hash_table_iter_next(&iter, (gpointer) &key, &value)) {
                GVariantBuilder *svc_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
                GVariantBuilder *service_properties_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

                g_variant_builder_add(service_properties_builder, "{sv}", "UUID", g_variant_new_string((char *) key));
                g_variant_builder_add(service_properties_builder, "{sv}", "Primary", g_variant_new_boolean(TRUE));

                g_variant_builder_add(svc_builder, "{sa{sv}}", GATT_SERV_INTERFACE, service_properties_builder);
                g_variant_builder_add(builder, "{oa{sa{sv}}}", (char *) value, svc_builder);
                g_variant_builder_unref(service_properties_builder);
                g_variant_builder_unref(svc_builder);
            }
        }

        GVariant *result = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&result, 1));
    }
}

static const GDBusInterfaceVTable application_method_table = {
        .method_call = bluez_application_method_call,
};

void binc_application_publish(Application *application, Adapter *adapter) {
    g_assert(application != NULL);
    g_assert(adapter != NULL);

    application->connection = binc_adapter_get_dbus_connection(adapter);

    GError *error = NULL;
    guint id = 0;
    GDBusNodeInfo *info = NULL;

    info = g_dbus_node_info_new_for_xml(manager_introspection_xml, &error);
    if (error) {
        log_debug(TAG, "Unable to create node: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    application->registration_id = g_dbus_connection_register_object(application->connection,
                                                                     application->path,
                                                                     info->interfaces[0],
                                                                     &application_method_table,
                                                                     application, NULL, &error);
    g_dbus_node_info_unref(info);
    if (application->registration_id == 0) {
        log_debug(TAG, "failed to publish application");
    } else {
        log_debug(TAG, "successfully published application");
    }
}

const char* binc_application_get_path(Application *application) {
    g_assert(application != NULL);
    return application->path;
}