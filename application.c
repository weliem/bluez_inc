//
// Created by martijn on 11-02-22.
//

#include "application.h"
#include "adapter.h"
#include "logger.h"
#include "characteristic.h"

#define BUFFER_SIZE 255
#define GATT_SERV_INTERFACE "org.bluez.GattService1"
#define GATT_CHAR_INTERFACE "org.bluez.GattCharacteristic1"

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


static const gchar characteristics_introspection_xml[] =
        "<node name='/'>"
        "  <interface name='org.bluez.GattCharacteristic1'>"
        "        <method name='ReadValue'>"
        "               <arg type='s' name='address' direction='in'/>"
        "               <arg type='u' name='id' direction='in'/>"
        "               <arg type='q' name='offset' direction='in'/>"
        "               <arg type='ay' name='Value' direction='out'/>"
        "        </method>"
        "        <method name='WriteValue'>"
        "               <arg type='s' name='address' direction='in'/>"
        "               <arg type='u' name='id' direction='in'/>"
        "               <arg type='q' name='offset' direction='in'/>"
        "               <arg type='b' name='response_needed' direction='in'/>"
        "               <arg type='ay' name='value' direction='in'/>"
        "        </method>"
        "        <method name='StartNotify'>"
        "        </method>"
        "        <method name='StopNotify'>"
        "        </method>"
        "        <method name='IndicateConfirm'>"
        "               <arg type='s' name='address' direction='in'/>"
        "               <arg type='b' name='complete' direction='in'/>"
        "        </method>"
        "  </interface>"
        "  <interface name='org.freedesktop.DBus.Properties'>"
        "    <property type='s' name='UUID' access='read'>"
        "    </property>"
        "    <property type='o' name='Service' access='read'>"
        "    </property>"
        "    <property type='ay' name='Value' access='readwrite'>"
        "    </property>"
        "        <property type='b' name='Notifying' access='read'>"
        "        </property>"
        "    <property type='as' name='Flags' access='read'>"
        "    </property>"
        "    <property type='s' name='Unicast' access='read'>"
        "    </property>"
        "        <property type='ao' name='Descriptors' access='read'>"
        "        </property>"
        "  </interface>"
        "</node>";

struct binc_application {
    char *path;
    guint registration_id;
    GDBusConnection *connection;
    GHashTable *services;
};

typedef struct binc_local_service {
    char* path;
    char* uuid;
    GHashTable *characteristics;
} LocalService;

void binc_local_service_free(LocalService *localService) {
    if (localService->path != NULL) {
        g_free(localService->path);
    }

    if (localService->uuid != NULL) {
        g_free(localService->uuid);
    }

    g_free(localService);
}

Application *binc_create_application() {
    Application *application = g_new0(Application, 1);
    application->path = g_strdup("/org/bluez/bincapplication");
    application->services = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                                  (GDestroyNotify) binc_local_service_free);
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

    LocalService *localService = g_new0(LocalService, 1);
    localService->uuid = g_strdup(service_uuid);
    localService->characteristics =  g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                          g_free);

    char path[BUFFER_SIZE];
    guint8 count = g_hash_table_size(application->services);
    g_snprintf(path, BUFFER_SIZE, "%s/service%d", application->path, count);
    localService->path = g_strdup(path);

    g_hash_table_insert(application->services, g_strdup(service_uuid), localService);
}

LocalService *binc_application_get_service(Application *application, const char *service_uuid) {
    g_assert(application != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));

    return g_hash_table_lookup(application->services, service_uuid);
}

typedef struct local_characteristic {
    char *service_uuid;
    char *uuid;
    char *path;
    guint8 permissions;
    GList *flags;
} LocalCharacteristic;

static GList *permissions2Flags(guint8 permissions) {
    GList *list = NULL;

    if (permissions & GATT_CHR_PROP_READ) {
        list = g_list_append(list, g_strdup("read"));
    }
    if (permissions & GATT_CHR_PROP_WRITE_WITHOUT_RESP) {
        list = g_list_append(list, g_strdup("write-without-response"));
    }
    if (permissions & GATT_CHR_PROP_WRITE) {
        list = g_list_append(list, g_strdup("write"));
    }
    if (permissions & GATT_CHR_PROP_NOTIFY) {
        list = g_list_append(list, g_strdup("notify"));
    }
    if (permissions & GATT_CHR_PROP_INDICATE) {
        list = g_list_append(list, g_strdup("indicate"));
    }
    return list;
}

void binc_application_add_characteristic(Application *application, const char *service_uuid,
                                         const char *characteristic_uuid, guint8 permissions) {
    g_assert(application != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));
    g_assert(g_uuid_string_is_valid(characteristic_uuid));

    LocalService *localService = binc_application_get_service(application, service_uuid);
    g_assert(localService != NULL);
    g_assert(localService->characteristics != NULL);

    LocalCharacteristic *characteristic = g_new0(LocalCharacteristic, 1);
    characteristic->service_uuid = g_strdup(service_uuid);
    characteristic->uuid = g_strdup(characteristic_uuid);
    characteristic->permissions = permissions;
    characteristic->flags = permissions2Flags(permissions);

    // Determine new path
    guint8 count = g_hash_table_size(localService->characteristics);
    char path[BUFFER_SIZE];
    g_snprintf(path, BUFFER_SIZE, "%s/char%d", localService->path, count);
    characteristic->path = g_strdup(path);
    g_hash_table_insert(localService->characteristics, g_strdup(characteristic_uuid), characteristic);
}

static GVariant* binc_local_service_get_characteristics(LocalService *localService) {
    g_assert(localService != NULL);

    GVariantBuilder *characteristics_builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, localService->characteristics);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        LocalCharacteristic *localCharacteristic = (LocalCharacteristic*) value;
//        log_debug(TAG, "adding %s", localCharacteristic->path);
        g_variant_builder_add(characteristics_builder, "o", localCharacteristic->path);
    }
    GVariant* result = g_variant_builder_end(characteristics_builder);
    g_variant_builder_unref(characteristics_builder);
    return result;
}

static GVariant* binc_local_characteristic_get_flags(LocalCharacteristic *localCharacteristic) {
    g_assert(localCharacteristic != NULL);

    GVariantBuilder *flags_builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

    for (GList *iterator = localCharacteristic->flags; iterator; iterator = iterator->next) {
        g_variant_builder_add(flags_builder, "s", (char*) iterator->data);
    }

    GVariant *result = g_variant_builder_end(flags_builder);
    g_variant_builder_unref(flags_builder);
    return result;
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
                LocalService *localService = (LocalService*) value;
                log_debug(TAG, "adding %s", localService->path);
                GVariantBuilder *service_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));

                // Build service properties
                GVariantBuilder *service_properties_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
                g_variant_builder_add(service_properties_builder, "{sv}", "UUID", g_variant_new_string((char *) key));
                g_variant_builder_add(service_properties_builder, "{sv}", "Primary", g_variant_new_boolean(TRUE));
                g_variant_builder_add(service_properties_builder, "{sv}", "Characteristics",
                                      binc_local_service_get_characteristics(localService));

                // Add the service to result
                g_variant_builder_add(service_builder, "{sa{sv}}", GATT_SERV_INTERFACE, service_properties_builder);
                g_variant_builder_unref(service_properties_builder);
                g_variant_builder_add(builder, "{oa{sa{sv}}}", localService->path, service_builder);
                g_variant_builder_unref(service_builder);

                // Build service characteristics

                GHashTableIter iter2;
                gpointer key2, value2;
                g_hash_table_iter_init(&iter2, localService->characteristics);
                while (g_hash_table_iter_next(&iter2, &key2, &value2)) {
                    LocalCharacteristic *localCharacteristic = (LocalCharacteristic*) value2;
                    log_debug(TAG, "adding %s", localCharacteristic->path);

                    GVariantBuilder *characteristic_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
                    GVariantBuilder *char_properties_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
                    g_variant_builder_add(char_properties_builder, "{sv}", "UUID", g_variant_new_string(localCharacteristic->uuid));
                    g_variant_builder_add(char_properties_builder, "{sv}", "Service", g_variant_new("o", localService->path));
                    g_variant_builder_add(char_properties_builder, "{sv}", "Flags", binc_local_characteristic_get_flags(localCharacteristic));

                    // Add the characteristic to result
                    g_variant_builder_add(characteristic_builder, "{sa{sv}}", GATT_CHAR_INTERFACE, char_properties_builder);
                    g_variant_builder_unref(char_properties_builder);
                    g_variant_builder_add(builder, "{oa{sa{sv}}}", localCharacteristic->path, characteristic_builder);
                    g_variant_builder_unref(characteristic_builder);

                    // Add descriptors
                    // NOTE that the CCCD is automatically added by Bluez so no need to add it.
                }

                // Build the final variant

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

const char *binc_application_get_path(Application *application) {
    g_assert(application != NULL);
    return application->path;
}