//
// Created by martijn on 11-02-22.
//

#include "application.h"
#include "adapter.h"
#include "logger.h"
#include "characteristic.h"
#include "utility.h"

#define BUFFER_SIZE 255
#define GATT_SERV_INTERFACE "org.bluez.GattService1"
#define GATT_CHAR_INTERFACE "org.bluez.GattCharacteristic1"

static const char *const TAG = "Application";

static const gchar manager_introspection_xml[] =
        "<node name='/'>"
        "  <interface name='org.freedesktop.DBus.ObjectManager'>"
        "    <method name='GetManagedObjects'>"
        "        <arg type='a{oa{sa{sv}}}' name='object_paths_interfaces_and_properties' direction='out'/>"
        "    </method>"
        "  </interface>"
        "</node>";

static const gchar service_introspection_xml[] =
        "<node name='/'>"
        "  <interface name='org.freedesktop.DBus.Properties'>"
        "        <property type='s' name='UUID' access='read' />"
        "        <property type='b' name='primary' access='read' />"
        "        <property type='o' name='Device' access='read' />"
        "        <property type='ao' name='Characteristics' access='read' />"
        "        <property type='s' name='Includes' access='read' />"
        "  </interface>"
        "</node>";


static const gchar characteristics_introspection_xml[] =
        "<node name='/'>"
        "  <interface name='org.bluez.GattCharacteristic1'>"
        "        <method name='ReadValue'>"
        "               <arg type='a{sv}' name='options' direction='in' />"
        "               <arg type='ay' name='value' direction='out'/>"
        "        </method>"
        "        <method name='WriteValue'>"
        "               <arg type='ay' name='value' direction='in'/>"
        "               <arg type='a{sv}' name='options' direction='in' />"
        "        </method>"
        "        <method name='StartNotify'/>"
        "        <method name='StopNotify' />"
        "        <method name='Confirm' />"
        "  </interface>"
        "  <interface name='org.freedesktop.DBus.Properties'>"
        "    <property type='s' name='UUID' access='read' />"
        "    <property type='o' name='Service' access='read' />"
        "    <property type='ay' name='Value' access='readwrite' />"
        "    <property type='b' name='Notifying' access='read' />"
        "    <property type='as' name='Flags' access='read' />"
        "    <property type='ao' name='Descriptors' access='read' />"
        "  </interface>"
        "</node>";

struct binc_application {
    char *path;
    guint registration_id;
    GDBusConnection *connection;
    GHashTable *services;
    onLocalCharacteristicWrite on_char_write;
    onLocalCharacteristicRead on_char_read;
    onLocalCharacteristicUpdated on_char_updated;
    onLocalCharacteristicStartNotify on_char_start_notify;
    onLocalCharacteristicStopNotify on_char_stop_notify;
};

typedef struct binc_local_service {
    char *path;
    char *uuid;
    guint registration_id;
    GHashTable *characteristics;
    Application *application;
} LocalService;

typedef struct local_characteristic {
    char *service_uuid;
    char *service_path;
    char *uuid;
    char *path;
    guint registration_id;
    GByteArray *value;
    guint8 permissions;
    GList *flags;
    gboolean notifying;
    Application *application;
} LocalCharacteristic;

static void binc_local_char_free(LocalCharacteristic *localCharacteristic) {
    g_assert(localCharacteristic != NULL);

    log_debug(TAG, "freeing characteristic %s", localCharacteristic->path);

    if (localCharacteristic->registration_id != 0) {
        gboolean result = g_dbus_connection_unregister_object(localCharacteristic->application->connection,
                                                              localCharacteristic->registration_id);
        if (!result) {
            log_debug(TAG, "error: could not unregister service %s", localCharacteristic->path);
        }
        localCharacteristic->registration_id = 0;
    }

    if (localCharacteristic->value != NULL) {
        g_byte_array_free(localCharacteristic->value, TRUE);
    }

    if (localCharacteristic->path != NULL) {
        g_free(localCharacteristic->path);
    }

    if (localCharacteristic->uuid != NULL) {
        g_free(localCharacteristic->uuid);
    }

    if (localCharacteristic->service_uuid != NULL) {
        g_free(localCharacteristic->service_uuid);
    }

    if (localCharacteristic->service_path != NULL) {
        g_free(localCharacteristic->service_path);
    }

    if (localCharacteristic->flags != NULL) {
        g_list_free_full(localCharacteristic->flags, g_free);
    }

    g_free(localCharacteristic);
}

void binc_local_service_free(LocalService *localService) {
    g_assert(localService != NULL);

    log_debug(TAG, "freeing service %s", localService->path);

    if (localService->characteristics != NULL) {
        g_hash_table_destroy(localService->characteristics);
        localService->characteristics = NULL;
    }

    if (localService->registration_id != 0) {
        gboolean result = g_dbus_connection_unregister_object(localService->application->connection,
                                                              localService->registration_id);
        if (!result) {
            log_debug(TAG, "error: could not unregister service %s", localService->path);
        }
        localService->registration_id = 0;
    }

    if (localService->path != NULL) {
        g_free(localService->path);
        localService->path = NULL;
    }

    if (localService->uuid != NULL) {
        g_free(localService->uuid);
        localService->uuid = NULL;
    }

    g_free(localService);
}

static GVariant *binc_local_service_get_characteristics(LocalService *localService) {
    g_assert(localService != NULL);

    GVariantBuilder *characteristics_builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, localService->characteristics);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        LocalCharacteristic *localCharacteristic = (LocalCharacteristic *) value;
        g_variant_builder_add(characteristics_builder, "o", localCharacteristic->path);
    }
    GVariant *result = g_variant_builder_end(characteristics_builder);
    g_variant_builder_unref(characteristics_builder);
    return result;
}

static GVariant *binc_local_characteristic_get_flags(LocalCharacteristic *localCharacteristic) {
    g_assert(localCharacteristic != NULL);

    GVariantBuilder *flags_builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

    for (GList *iterator = localCharacteristic->flags; iterator; iterator = iterator->next) {
        g_variant_builder_add(flags_builder, "s", (char *) iterator->data);
    }

    GVariant *result = g_variant_builder_end(flags_builder);
    g_variant_builder_unref(flags_builder);
    return result;
}

static void binc_internal_application_method_call(GDBusConnection *conn,
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
                LocalService *localService = (LocalService *) value;
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
                    LocalCharacteristic *localCharacteristic = (LocalCharacteristic *) value2;
                    log_debug(TAG, "adding %s", localCharacteristic->path);

                    GVariantBuilder *characteristic_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
                    GVariantBuilder *char_properties_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

                    // Build characteristic properties
                    GByteArray *byteArray = localCharacteristic->value;
                    GVariant *byteArrayVariant = NULL;
                    if (byteArray != NULL) {
                        byteArrayVariant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, byteArray->data,
                                                                     byteArray->len, sizeof(guint8));
                        g_variant_builder_add(char_properties_builder, "{sv}", "Value", byteArrayVariant);
                    }
                    g_variant_builder_add(char_properties_builder, "{sv}", "UUID",
                                          g_variant_new_string(localCharacteristic->uuid));
                    g_variant_builder_add(char_properties_builder, "{sv}", "Service",
                                          g_variant_new("o", localService->path));
                    g_variant_builder_add(char_properties_builder, "{sv}", "Flags",
                                          binc_local_characteristic_get_flags(localCharacteristic));
                    g_variant_builder_add(char_properties_builder, "{sv}", "Notifying",
                                          g_variant_new("b", localCharacteristic->notifying));

                    // Add the characteristic to result
                    g_variant_builder_add(characteristic_builder, "{sa{sv}}", GATT_CHAR_INTERFACE,
                                          char_properties_builder);
                    g_variant_builder_unref(char_properties_builder);
                    g_variant_builder_add(builder, "{oa{sa{sv}}}", localCharacteristic->path, characteristic_builder);
                    g_variant_builder_unref(characteristic_builder);

                    // TODO Add descriptors
                    // NOTE that the CCCD is automatically added by Bluez so no need to add it.
                }
            }
        }

        // Build the final variant
        GVariant *result = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&result, 1));
    }
}

static const GDBusInterfaceVTable application_method_table = {
        .method_call = binc_internal_application_method_call,
};

void binc_application_publish(Application *application, const Adapter *adapter) {
    g_assert(application != NULL);
    g_assert(adapter != NULL);

    GError *error = NULL;
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

Application *binc_create_application(const Adapter *adapter) {
    g_assert(adapter != NULL);

    Application *application = g_new0(Application, 1);
    application->connection = binc_adapter_get_dbus_connection(adapter);
    application->path = g_strdup("/org/bluez/bincapplication");
    application->services = g_hash_table_new_full(g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  (GDestroyNotify) binc_local_service_free);

    binc_application_publish(application, adapter);

    return application;
}

void binc_application_free(Application *application) {
    g_assert(application != NULL);

    log_debug(TAG, "freeing application %s", application->path);

    if (application->services != NULL) {
        g_hash_table_destroy(application->services);
        application->services = NULL;
    }

    if (application->registration_id != 0) {
        gboolean result = g_dbus_connection_unregister_object(application->connection, application->registration_id);
        if (!result) {
            log_debug(TAG, "error: could not unregister application %s", application->path);
        }
        application->registration_id = 0;
    }

    if (application->path != NULL) {
        g_free(application->path);
        application->path = NULL;
    }

    g_free(application);
}

static const GDBusInterfaceVTable service_table = {};

void binc_application_add_service(Application *application, const char *service_uuid) {
    g_assert(application != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));

    LocalService *localService = g_new0(LocalService, 1);
    localService->uuid = g_strdup(service_uuid);
    localService->application = application;
    localService->characteristics = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                                          (GDestroyNotify) binc_local_char_free);

    char path[BUFFER_SIZE];
    guint8 count = g_hash_table_size(application->services);
    g_snprintf(path, BUFFER_SIZE, "%s/service%d", application->path, count);
    localService->path = g_strdup(path);

    g_hash_table_insert(application->services, g_strdup(service_uuid), localService);

    GError *error = NULL;
    guint id = 0;
    GDBusNodeInfo *info = NULL;

    info = g_dbus_node_info_new_for_xml(service_introspection_xml, &error);
    if (error) {
        log_debug(TAG, "Unable to create node: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    localService->registration_id = g_dbus_connection_register_object(application->connection,
                                                                      localService->path,
                                                                      info->interfaces[0],
                                                                      &service_table,
                                                                      localService, NULL, &error);
    g_dbus_node_info_unref(info);

    if (localService->registration_id == 0) {
        log_debug(TAG, "failed to publish local service");
        log_debug(TAG, "Error %s", error->message);
    } else {
        log_debug(TAG, "successfully published local service");

    }
}

static LocalService *binc_application_get_service(const Application *application, const char *service_uuid) {
    g_assert(application != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));

    return g_hash_table_lookup(application->services, service_uuid);
}


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


static int binc_characteristic_set_value(const Application *application, LocalCharacteristic *characteristic, GByteArray *byteArray) {
    g_return_val_if_fail (characteristic != NULL, EINVAL);
    g_return_val_if_fail (byteArray != NULL, EINVAL);

    GString *byteArrayStr = g_byte_array_as_hex(byteArray);
    log_debug(TAG, "set value <%s> to <%s>", byteArrayStr->str, characteristic->uuid);
    g_string_free(byteArrayStr, TRUE);

    if (characteristic->value != NULL) {
        g_byte_array_free(characteristic->value, TRUE);
    }
    characteristic->value = byteArray;

    if (application->on_char_updated != NULL) {
        application->on_char_updated(characteristic->application, characteristic->service_uuid,
                                            characteristic->uuid, byteArray);
    }

    return 0;
}

static LocalCharacteristic *get_local_characteristic(const Application *application, const char *service_uuid,
                                                     const char *characteristic_uuid) {
    LocalService *service = binc_application_get_service(application, service_uuid);
    if (service != NULL) {
        return g_hash_table_lookup(service->characteristics, characteristic_uuid);
    }
    return NULL;
}

int binc_application_set_char_value(const Application *application, const char *service_uuid,
                                     const char *char_uuid, GByteArray *byteArray) {

    g_return_val_if_fail (application != NULL, EINVAL);
    g_return_val_if_fail (service_uuid != NULL, EINVAL);
    g_return_val_if_fail (char_uuid != NULL, EINVAL);
    g_return_val_if_fail (byteArray != NULL, EINVAL);
    g_return_val_if_fail (g_uuid_string_is_valid(service_uuid), EINVAL);
    g_return_val_if_fail (g_uuid_string_is_valid(char_uuid), EINVAL);

    LocalCharacteristic *characteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (characteristic == NULL) {
        g_critical("%s: characteristic with uuid %s does not exist", G_STRFUNC, char_uuid);
        return EINVAL;
    }

    return binc_characteristic_set_value(application,characteristic, byteArray);
}

GByteArray *binc_application_get_char_value(const Application *application, const char *service_uuid,
                                            const char *char_uuid) {

    g_return_val_if_fail (application != NULL, NULL);
    g_return_val_if_fail (service_uuid != NULL, NULL);
    g_return_val_if_fail (char_uuid != NULL, NULL);
    g_return_val_if_fail (g_uuid_string_is_valid(service_uuid), NULL);
    g_return_val_if_fail (g_uuid_string_is_valid(char_uuid), NULL);

    LocalCharacteristic *characteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (characteristic != NULL) {
        return characteristic->value;
    }
    return NULL;
}

static void binc_internal_characteristic_method_call(GDBusConnection *conn,
                                                     const gchar *sender,
                                                     const gchar *path,
                                                     const gchar *interface,
                                                     const gchar *method,
                                                     GVariant *params,
                                                     GDBusMethodInvocation *invocation,
                                                     void *userdata) {

    log_debug(TAG, "local characteristic method called: %s", method);
    LocalCharacteristic *characteristic = (LocalCharacteristic *) userdata;
    g_assert(characteristic != NULL);

    Application *application = characteristic->application;
    g_assert(application != NULL);

    if (g_str_equal(method, "ReadValue")) {
        g_assert(g_str_equal(g_variant_get_type_string(params), "(a{sv})"));
        char *device = NULL;
        guint16 mtu = 0;
        guint16 offset = 0;
        char *link_type = NULL;

        // Parse options
        GVariantIter *optionsVariant;
        g_variant_get(params, "(a{sv})", &optionsVariant);

        // Parse options
        GVariant *property_value;
        gchar *property_name;
        while (g_variant_iter_loop(optionsVariant, "{&sv}", &property_name, &property_value)) {
            //log_debug(TAG, "property %s, variant type %s", property_name, g_variant_get_type_string(property_value));

            if (g_str_equal(property_name, "offset")) {
                offset = g_variant_get_uint16(property_value);
            } else if (g_str_equal(property_name, "mtu")) {
                mtu = g_variant_get_uint16(property_value);
            } else if (g_str_equal(property_name, "device")) {
                device = g_strdup(g_variant_get_string(property_value, NULL));
            } else if (g_str_equal(property_name, "link")) {
                link_type = g_strdup(g_variant_get_string(property_value, NULL));
            }
        }
        g_variant_iter_free(optionsVariant);

        log_debug(TAG, "read with offset=%u, mtu=%u, link=%s, device=%s", (unsigned int) offset,
                  (unsigned int) mtu, link_type, device);

        // Clean up
        if (link_type != NULL) g_free(link_type);
        if (device != NULL) g_free(device);

        // Allow application to set the characteristic value before returning it
        if (application->on_char_read != NULL) {
            application->on_char_read(characteristic->application, device, characteristic->service_uuid,
                                      characteristic->uuid);
        }

        // Return the characteristic's value
        GVariant *result = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                     characteristic->value->data,
                                                     characteristic->value->len,
                                                     sizeof(guint8));
        g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&result, 1));
        return;
    } else if (g_str_equal(method, "WriteValue")) {
        g_assert(g_str_equal(g_variant_get_type_string(params), "(aya{sv})"));
        guint16 offset = 0;
        guint16 mtu = 0;
        char *write_type = NULL;
        char *link_type = NULL;
        char *device = NULL;

        GVariant *valueVariant, *optionsVariant;
        g_variant_get(params, "(@ay@a{sv})", &valueVariant, &optionsVariant);

        // Parse options
        GVariantIter iter;
        GVariant *property_value;
        gchar *property_name;
        g_variant_iter_init(&iter, optionsVariant);
        while (g_variant_iter_loop(&iter, "{&sv}", &property_name, &property_value)) {
            //log_debug(TAG, "property %s, variant type %s", property_name, g_variant_get_type_string(property_value));

            if (g_str_equal(property_name, "offset")) {
                offset = g_variant_get_uint16(property_value);
            } else if (g_str_equal(property_name, "type")) {
                write_type = g_strdup(g_variant_get_string(property_value, NULL));
            } else if (g_str_equal(property_name, "mtu")) {
                mtu = g_variant_get_uint16(property_value);
            } else if (g_str_equal(property_name, "link")) {
                link_type = g_strdup(g_variant_get_string(property_value, NULL));
            } else if (g_str_equal(property_name, "device")) {
                device = g_strdup(g_variant_get_string(property_value, NULL));
            }
        }
        log_debug(TAG, "write with offset=%u, type=%s, mtu=%u, link=%s, device=%s", (unsigned int) offset, write_type,
                  (unsigned int) mtu, link_type, device);

        // Clean up
        if (link_type != NULL) g_free(link_type);
        if (device != NULL) g_free(device);
        if (write_type != NULL) g_free(write_type);

        // Get byte array
        size_t data_length = 0;
        guint8 *data = (guint8 *) g_variant_get_fixed_array(valueVariant, &data_length, sizeof(guint8));
        GByteArray *byteArray = g_byte_array_sized_new(data_length);

        // Allow application to set the characteristic value before returning it
        char* result = NULL;
        if (application->on_char_write != NULL) {
            result = application->on_char_write(characteristic->application, device, characteristic->service_uuid,
                                      characteristic->uuid, byteArray);
        }

        if (result) {
            g_dbus_method_invocation_return_dbus_error(invocation, result, "write error");
            log_debug(TAG, "write error");
            return;
        }

        // Store the new byte array
        g_byte_array_append(byteArray, data, data_length);
        binc_characteristic_set_value(application, characteristic, byteArray);

        // Send properties changed signal with new value
        binc_application_notify(application, characteristic->service_uuid, characteristic->uuid, byteArray);

        // Only send a response for a Write With Response. How can we tell????
        g_dbus_method_invocation_return_value(invocation, g_variant_new ("()"));
    } else if (g_str_equal(method, "StartNotify")) {
        characteristic->notifying = TRUE;
        g_dbus_method_invocation_return_value(invocation, NULL);

//        if (characteristic->value != NULL) {
//            binc_application_notify(application,
//                                    characteristic->service_uuid,
//                                    characteristic->uuid,
//                                    characteristic->value);
//        }

        if (application->on_char_start_notify != NULL) {
            application->on_char_start_notify(characteristic->application, characteristic->service_uuid,
                                      characteristic->uuid);
        }
    } else if (g_str_equal(method, "StopNotify")) {
        characteristic->notifying = FALSE;
        g_dbus_method_invocation_return_value(invocation, NULL);

        if (application->on_char_stop_notify != NULL) {
            application->on_char_stop_notify(characteristic->application, characteristic->service_uuid,
                                              characteristic->uuid);
        }
    }
}

GVariant *characteristic_get_property(GDBusConnection *connection,
                                      const gchar *sender,
                                      const gchar *object_path,
                                      const gchar *interface_name,
                                      const gchar *property_name,
                                      GError **error,
                                      gpointer user_data) {

    log_debug(TAG, "local characteristic get property : %s", property_name);
    LocalCharacteristic *characteristic = (LocalCharacteristic *) user_data;
    g_assert(characteristic != NULL);

    GVariant *ret;
    if (g_str_equal(property_name, "UUID")) {
        ret = g_variant_new_string(characteristic->uuid);
    } else if (g_str_equal(property_name, "Service")) {
        ret = g_variant_new_object_path(characteristic->path);
    } else if (g_str_equal(property_name, "Flags")) {
        ret = binc_local_characteristic_get_flags(characteristic);
    } else if (g_str_equal(property_name, "Notifying")) {
        ret = g_variant_new_boolean(characteristic->notifying);
    } else if (g_str_equal(property_name, "Value")) {
        ret = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, characteristic->value->data, characteristic->value->len,
                                        sizeof(guint8));
    }
    return ret;
}

static const GDBusInterfaceVTable characteristic_table = {
        .method_call = binc_internal_characteristic_method_call,
        .get_property = characteristic_get_property
};

void binc_application_add_characteristic(Application *application, const char *service_uuid,
                                         const char *char_uuid, guint8 permissions) {
    g_assert(application != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));
    g_assert(g_uuid_string_is_valid(char_uuid));

    LocalService *localService = binc_application_get_service(application, service_uuid);
    g_assert(localService != NULL);
    g_assert(localService->characteristics != NULL);

    LocalCharacteristic *characteristic = g_new0(LocalCharacteristic, 1);
    characteristic->service_uuid = g_strdup(service_uuid);
    characteristic->service_path = g_strdup(localService->path);
    characteristic->uuid = g_strdup(char_uuid);
    characteristic->permissions = permissions;
    characteristic->flags = permissions2Flags(permissions);
    characteristic->value = NULL;
    characteristic->application = application;

    // Determine new path
    guint8 count = g_hash_table_size(localService->characteristics);
    char path[BUFFER_SIZE];
    g_snprintf(path, BUFFER_SIZE, "%s/char%d", localService->path, count);
    characteristic->path = g_strdup(path);
    g_hash_table_insert(localService->characteristics, g_strdup(char_uuid), characteristic);

    GError *error = NULL;
    GDBusNodeInfo *info = NULL;
    info = g_dbus_node_info_new_for_xml(characteristics_introspection_xml, &error);
    if (error) {
        log_debug(TAG, "Unable to create node: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    characteristic->registration_id = g_dbus_connection_register_object(application->connection,
                                                                        characteristic->path,
                                                                        info->interfaces[0],
                                                                        &characteristic_table,
                                                                        characteristic, NULL, &error);
    g_dbus_node_info_unref(info);

    if (characteristic->registration_id == 0) {
        log_debug(TAG, "failed to publish local characteristic");
        log_debug(TAG, "Error %s", error->message);
    } else {
        log_debug(TAG, "successfully published local characteristic");
    }

    if (error) {
        g_clear_error(&error);
    }
}


const char *binc_application_get_path(Application *application) {
    g_assert(application != NULL);
    return application->path;
}

void
binc_application_set_char_read_cb(Application *application, onLocalCharacteristicRead callback) {
    g_assert(application != NULL);
    g_assert(callback != NULL);

    application->on_char_read = callback;
}

void
binc_application_set_char_write_cb(Application *application, onLocalCharacteristicWrite callback) {
    g_assert(application != NULL);
    g_assert(callback != NULL);

    application->on_char_write = callback;
}

void binc_application_set_char_start_notify_cb(Application *application, onLocalCharacteristicStartNotify callback) {
    g_assert(application != NULL);
    g_assert(callback != NULL);

    application->on_char_start_notify = callback;
}

void binc_application_set_char_stop_notify_cb(Application *application, onLocalCharacteristicStopNotify callback) {
    g_assert(application != NULL);
    g_assert(callback != NULL);

    application->on_char_stop_notify = callback;
}

void binc_application_notify(const Application *application, const char *service_uuid, const char *char_uuid,
                             GByteArray *byteArray) {
    g_assert(application != NULL);
    g_assert(service_uuid != NULL);
    g_assert(char_uuid != NULL);
    g_assert(byteArray != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));
    g_assert(g_uuid_string_is_valid(char_uuid));

    LocalCharacteristic *characteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (characteristic == NULL) {
        log_debug(TAG, "cannot notify, characteristic <%s> not found", char_uuid);
        return;
    }

    GVariant *valueVariant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                       byteArray->data,
                                                       byteArray->len,
                                                       sizeof(guint8));
    GVariantBuilder *properties_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(properties_builder, "{sv}", "Value", valueVariant);
    GVariantBuilder *invalidated_builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

    GError *error = NULL;
    gboolean result = g_dbus_connection_emit_signal(application->connection,
                                                    NULL,
                                                    characteristic->path,
                                                    "org.freedesktop.DBus.Properties",
                                                    "PropertiesChanged",
                                                    g_variant_new("(sa{sv}as)", "org.bluez.GattCharacteristic1",
                                                                  properties_builder, invalidated_builder),
                                                    &error);

    if (result != TRUE) {
        if (error != NULL) {
            log_debug(TAG, "error emitting signal: %s", error->message);
            g_clear_error(&error);
        }
    } else {
        GString *byteArrayStr = g_byte_array_as_hex(byteArray);
        log_debug(TAG, "notified <%s> on <%s>", byteArrayStr->str, characteristic->uuid);
        g_string_free(byteArrayStr, TRUE);
    }

    g_variant_builder_unref(invalidated_builder);
    g_variant_builder_unref(properties_builder);
}