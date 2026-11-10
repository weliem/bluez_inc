/*
 *   Copyright (c) 2022 Martijn van Welie
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

#include "application.h"
#include "adapter.h"
#include "logger.h"
#include "characteristic.h"
#include "utility.h"
#include <errno.h>
#include <glib-unix.h>
#include <gio/gunixfdlist.h>
#include <sys/socket.h>
#include <unistd.h>

#define GATT_SERV_INTERFACE "org.bluez.GattService1"
#define GATT_CHAR_INTERFACE "org.bluez.GattCharacteristic1"
#define GATT_DESC_INTERFACE "org.bluez.GattDescriptor1"

static const char *const TAG = "Application";

static const char *const CHARACTERISTIC_METHOD_READ_VALUE = "ReadValue";
static const char *const CHARACTERISTIC_METHOD_WRITE_VALUE = "WriteValue";
static const char *const CHARACTERISTIC_METHOD_STOP_NOTIFY = "StopNotify";
static const char *const CHARACTERISTIC_METHOD_START_NOTIFY = "StartNotify";
static const char *const CHARACTERISTIC_METHOD_CONFIRM = "Confirm";
static const char *const DESCRIPTOR_METHOD_READ_VALUE = "ReadValue";
static const char *const DESCRIPTOR_METHOD_WRITE_VALUE = "WriteValue";

static const gchar object_manager_xml[] =
        "<node name='/'>"
        "  <interface name='org.freedesktop.DBus.ObjectManager'>"
        "    <method name='GetManagedObjects'>"
        "        <arg type='a{oa{sa{sv}}}' name='object_paths_interfaces_and_properties' direction='out'/>"
        "    </method>"
        "  </interface>"
        "</node>";

static const gchar service_xml[] =
        "<node name='/'>"
    "  <interface name='org.bluez.GattService1'>"
        "        <property type='s' name='UUID' access='read' />"
    "        <property type='b' name='Primary' access='read' />"
        "        <property type='ao' name='Characteristics' access='read' />"
    "        <property type='q' name='Handle' access='readwrite' />"
        "  </interface>"
        "</node>";

static const gchar characteristic_xml[] =
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
        "        <method name='AcquireNotify'>"
        "               <arg type='a{sv}' name='options' direction='in' />"
        "               <arg type='h' name='fd' direction='out' />"
        "               <arg type='q' name='mtu' direction='out' />"
        "        </method>"
        "        <method name='StopNotify' />"
        "        <method name='Confirm' />"
        "    <property type='s' name='UUID' access='read' />"
        "    <property type='o' name='Service' access='read' />"
        "    <property type='ay' name='Value' access='readwrite' />"
        "    <property type='b' name='Notifying' access='read' />"
        "    <property type='b' name='NotifyAcquired' access='read' />"
        "    <property type='as' name='Flags' access='read' />"
        "    <property type='ao' name='Descriptors' access='read' />"
        "    <property type='q' name='Handle' access='readwrite' />"
        "  </interface>"
        "</node>";

static const gchar descriptor_xml[] =
        "<node name='/'>"
        "  <interface name='org.bluez.GattDescriptor1'>"
        "        <method name='ReadValue'>"
        "               <arg type='a{sv}' name='options' direction='in' />"
        "               <arg type='ay' name='value' direction='out'/>"
        "        </method>"
        "        <method name='WriteValue'>"
        "               <arg type='ay' name='value' direction='in'/>"
        "               <arg type='a{sv}' name='options' direction='in' />"
        "        </method>"
        "    <property type='s' name='UUID' access='read' />"
        "    <property type='o' name='Characteristic' access='read' />"
        "    <property type='ay' name='Value' access='readwrite' />"
        "    <property type='as' name='Flags' access='read' />"
        "    <property type='q' name='Handle' access='readwrite' />"
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
    onLocalCharacteristicNotifyState on_char_notify_state;
    onLocalDescriptorWrite on_desc_write;
    onLocalDescriptorRead on_desc_read;
    void *user_data; // Borrowed
};

typedef struct binc_local_service {
    char *path;
    char *uuid;
    guint16 handle;
    guint registration_id;
    GHashTable *characteristics;
    Application *application;
} LocalService;

typedef struct local_characteristic {
    char *service_uuid;
    char *service_path;
    char *uuid;
    char *path;
    guint16 handle;
    guint registration_id;
    GByteArray *value;
    guint permissions;
    GList *flags;
    gboolean notifying;
    GHashTable *descriptors;
    GHashTable *notify_clients;
    GMutex notify_mutex;
    Application *application;
} LocalCharacteristic;

typedef struct {
    LocalCharacteristic *characteristic;
    char *address;
    int fd;
    guint16 mtu;
    GSource *source;
} LocalNotifyClient;

static void notify_client_free(LocalNotifyClient *client) {
    if (client->source != NULL) {
        g_source_destroy(client->source);
        g_source_unref(client->source);
    }
    close(client->fd);
    g_free(client->address);
    g_free(client);
}

typedef struct local_descriptor {
    char *path;
    char *char_path;
    char *uuid;
    char *char_uuid;
    char *service_uuid;
    guint16 handle;
    guint registration_id;
    GByteArray *value;
    guint permissions;
    GList *flags;
    Application *application;
} LocalDescriptor;

static void binc_local_desc_free(LocalDescriptor *localDescriptor) {
    g_assert(localDescriptor != NULL);

    log_debug(TAG, "freeing descriptor %s", localDescriptor->path);

    if (localDescriptor->registration_id != 0) {
        gboolean result = g_dbus_connection_unregister_object(localDescriptor->application->connection,
                                                              localDescriptor->registration_id);
        if (!result) {
            log_debug(TAG, "error: could not unregister descriptor %s", localDescriptor->path);
        }
        localDescriptor->registration_id = 0;
    }

    if (localDescriptor->value != NULL) {
        g_byte_array_free(localDescriptor->value, TRUE);
        localDescriptor->value = NULL;
    }

    g_free(localDescriptor->path);
    localDescriptor->path = NULL;

    g_free(localDescriptor->char_path);
    localDescriptor->char_path = NULL;

    g_free(localDescriptor->uuid);
    localDescriptor->uuid = NULL;

    g_free(localDescriptor->char_uuid);
    localDescriptor->char_uuid = NULL;

    g_free(localDescriptor->service_uuid);
    localDescriptor->service_uuid = NULL;

    if (localDescriptor->flags != NULL) {
        g_list_free_full(localDescriptor->flags, g_free);
        localDescriptor->flags = NULL;
    }

    g_free(localDescriptor);
}

static void binc_local_char_free(LocalCharacteristic *localCharacteristic) {
    g_assert(localCharacteristic != NULL);

    log_debug(TAG, "freeing characteristic %s", localCharacteristic->path);

    if (localCharacteristic->notify_clients != NULL) {
        g_hash_table_destroy(localCharacteristic->notify_clients);
    }
    g_mutex_clear(&localCharacteristic->notify_mutex);

    if (localCharacteristic->descriptors != NULL) {
        g_hash_table_destroy(localCharacteristic->descriptors);
        localCharacteristic->descriptors = NULL;
    }

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
        localCharacteristic->value = NULL;
    }

    g_free(localCharacteristic->path);
    localCharacteristic->path = NULL;

    g_free(localCharacteristic->uuid);
    localCharacteristic->uuid = NULL;

    g_free(localCharacteristic->service_uuid);
    localCharacteristic->service_uuid = NULL;

    g_free(localCharacteristic->service_path);
    localCharacteristic->service_path = NULL;

    if (localCharacteristic->flags != NULL) {
        g_list_free_full(localCharacteristic->flags, g_free);
        localCharacteristic->flags = NULL;
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

    g_free(localService->path);
    localService->path = NULL;

    g_free(localService->uuid);
    localService->uuid = NULL;

    g_free(localService);
}

typedef struct read_options {
    char *device;
    guint16 mtu;
    guint16 offset;
    char *link_type;
} ReadOptions;

void read_options_free(ReadOptions *options) {
    if (options->link_type != NULL) g_free(options->link_type);
    if (options->device != NULL) g_free(options->device);
    g_free(options);
}

static ReadOptions *parse_read_options(GVariant *params) {
    g_assert(g_str_equal(g_variant_get_type_string(params), "(a{sv})"));
    ReadOptions *options = g_new0(ReadOptions, 1);

    GVariantIter *optionsVariant;
    g_variant_get(params, "(a{sv})", &optionsVariant);

    GVariant *property_value;
    gchar *property_name;
    while (g_variant_iter_loop(optionsVariant, "{&sv}", &property_name, &property_value)) {
        if (g_str_equal(property_name, "offset")) {
            options->offset = g_variant_get_uint16(property_value);
        } else if (g_str_equal(property_name, "mtu")) {
            options->mtu = g_variant_get_uint16(property_value);
        } else if (g_str_equal(property_name, "device")) {
            options->device = path_to_address(g_variant_get_string(property_value, NULL));
        } else if (g_str_equal(property_name, "link")) {
            options->link_type = g_strdup(g_variant_get_string(property_value, NULL));
        }
    }
    g_variant_iter_free(optionsVariant);

    log_debug(TAG, "read with offset=%u, mtu=%u, link=%s, device=%s", (unsigned int) options->offset,
              (unsigned int) options->mtu, options->link_type, options->device);

    return options;
}

typedef struct write_options {
    char *write_type;
    char *device;
    guint16 mtu;
    guint16 offset;
    char *link_type;
} WriteOptions;

void write_options_free(WriteOptions *options) {
    if (options->link_type != NULL) g_free(options->link_type);
    if (options->device != NULL) g_free(options->device);
    if (options->write_type != NULL) g_free(options->write_type);
    g_free(options);
}

static WriteOptions *parse_write_options(GVariant *optionsVariant) {
    g_assert(g_str_equal(g_variant_get_type_string(optionsVariant), "a{sv}"));
    WriteOptions *options = g_new0(WriteOptions, 1);

    GVariantIter iter;
    g_variant_iter_init(&iter, optionsVariant);
    GVariant *property_value;
    gchar *property_name;
    while (g_variant_iter_loop(&iter, "{&sv}", &property_name, &property_value)) {
        if (g_str_equal(property_name, "offset")) {
            options->offset = g_variant_get_uint16(property_value);
        } else if (g_str_equal(property_name, "type")) {
            options->write_type = g_strdup(g_variant_get_string(property_value, NULL));
        } else if (g_str_equal(property_name, "mtu")) {
            options->mtu = g_variant_get_uint16(property_value);
        } else if (g_str_equal(property_name, "device")) {
            options->device = path_to_address(g_variant_get_string(property_value, NULL));
        } else if (g_str_equal(property_name, "link")) {
            options->link_type = g_strdup(g_variant_get_string(property_value, NULL));
        }
    }

    log_debug(TAG, "write with offset=%u, mtu=%u, link=%s, device=%s", (unsigned int) options->offset,
              (unsigned int) options->mtu, options->link_type, options->device);

    return options;
}

static void add_char_path(gpointer key, gpointer value, gpointer userdata) {
    LocalCharacteristic *localCharacteristic = (LocalCharacteristic *) value;
    g_variant_builder_add((GVariantBuilder *) userdata, "o", localCharacteristic->path);
}

static GVariant *binc_local_service_get_characteristics(const LocalService *localService) {
    g_assert(localService != NULL);

    GVariantBuilder *characteristics_builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
    g_hash_table_foreach(localService->characteristics, add_char_path, characteristics_builder);
    GVariant *result = g_variant_builder_end(characteristics_builder);
    g_variant_builder_unref(characteristics_builder);
    return result;
}

static void add_desc_path(gpointer key, gpointer value, gpointer userdata) {
    LocalDescriptor *localDescriptor = (LocalDescriptor *) value;
    g_variant_builder_add((GVariantBuilder *) userdata, "o", localDescriptor->path);
}

static GVariant *binc_local_characteristic_get_descriptors(const LocalCharacteristic *localCharacteristic) {
    g_assert(localCharacteristic != NULL);

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
    g_hash_table_foreach(localCharacteristic->descriptors, add_desc_path, builder);
    GVariant *result = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return result;
}

static GVariant *binc_local_characteristic_get_flags(const LocalCharacteristic *localCharacteristic) {
    g_assert(localCharacteristic != NULL);

    GVariantBuilder *flags_builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    for (GList *iterator = localCharacteristic->flags; iterator; iterator = iterator->next) {
        g_variant_builder_add(flags_builder, "s", (char *) iterator->data);
    }
    GVariant *result = g_variant_builder_end(flags_builder);
    g_variant_builder_unref(flags_builder);
    return result;
}

static GVariant *binc_local_descriptor_get_flags(const LocalDescriptor *localDescriptor) {
    g_assert(localDescriptor != NULL);

    GVariantBuilder *flags_builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    for (GList *iterator = localDescriptor->flags; iterator; iterator = iterator->next) {
        g_variant_builder_add(flags_builder, "s", (char *) iterator->data);
    }
    GVariant *result = g_variant_builder_end(flags_builder);
    g_variant_builder_unref(flags_builder);
    return result;
}

static void add_descriptors(GVariantBuilder *builder,
                            LocalCharacteristic *localCharacteristic) {
    // NOTE that the CCCD is automatically added by Bluez so no need to add it.
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, localCharacteristic->descriptors);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        LocalDescriptor *localDescriptor = (LocalDescriptor *) value;
        log_debug(TAG, "adding %s", localDescriptor->path);

        GVariantBuilder *descriptors_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
        GVariantBuilder *desc_properties_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

        GByteArray *byteArray = localDescriptor->value;
        GVariant *byteArrayVariant = NULL;
        if (byteArray != NULL) {
            byteArrayVariant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, byteArray->data,
                                                         byteArray->len, sizeof(guint8));
            g_variant_builder_add(desc_properties_builder, "{sv}", "Value", byteArrayVariant);
        }
        g_variant_builder_add(desc_properties_builder, "{sv}", "UUID",
                              g_variant_new_string(localDescriptor->uuid));
        g_variant_builder_add(desc_properties_builder, "{sv}", "Handle",
                      g_variant_new_uint16(localDescriptor->handle));
        g_variant_builder_add(desc_properties_builder, "{sv}", "Characteristic",
                              g_variant_new("o", localDescriptor->char_path));
        g_variant_builder_add(desc_properties_builder, "{sv}", "Flags",
                              binc_local_descriptor_get_flags(localDescriptor));

        // Add the descriptor to result
        g_variant_builder_add(descriptors_builder, "{sa{sv}}", GATT_DESC_INTERFACE,
                              desc_properties_builder);
        g_variant_builder_unref(desc_properties_builder);
        g_variant_builder_add(builder, "{oa{sa{sv}}}", localDescriptor->path, descriptors_builder);
        g_variant_builder_unref(descriptors_builder);
    }
}

static void add_characteristics(GVariantBuilder *builder, LocalService *localService) {
    // Build service characteristics
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, localService->characteristics);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        LocalCharacteristic *localCharacteristic = (LocalCharacteristic *) value;
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
        g_variant_builder_add(char_properties_builder, "{sv}", "Handle",
                      g_variant_new_uint16(localCharacteristic->handle));
        g_variant_builder_add(char_properties_builder, "{sv}", "Service",
                              g_variant_new("o", localService->path));
        g_variant_builder_add(char_properties_builder, "{sv}", "Flags",
                              binc_local_characteristic_get_flags(localCharacteristic));
        g_variant_builder_add(char_properties_builder, "{sv}", "Notifying",
                              g_variant_new("b", localCharacteristic->notifying));
        if ((localCharacteristic->permissions & GATT_CHR_PROP_NOTIFY) &&
                localCharacteristic->application->on_char_notify_state != NULL) {
            g_variant_builder_add(char_properties_builder, "{sv}", "NotifyAcquired",
                                  g_variant_new_boolean(g_hash_table_size(localCharacteristic->notify_clients) > 0));
        }
        g_variant_builder_add(char_properties_builder, "{sv}", "Descriptors",
                              binc_local_characteristic_get_descriptors(localCharacteristic));

        // Add the characteristic to result
        g_variant_builder_add(characteristic_builder, "{sa{sv}}", GATT_CHAR_INTERFACE,
                              char_properties_builder);
        g_variant_builder_unref(char_properties_builder);
        g_variant_builder_add(builder, "{oa{sa{sv}}}", localCharacteristic->path, characteristic_builder);
        g_variant_builder_unref(characteristic_builder);

        add_descriptors(builder, localCharacteristic);
    }
}

static void add_services(Application *application, GVariantBuilder *builder) {
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, application->services);
    while (g_hash_table_iter_next(&iter, (gpointer) &key, &value)) {
        LocalService *localService = (LocalService *) value;
        log_debug(TAG, "adding %s", localService->path);
        GVariantBuilder *service_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));

        // Build service properties
        GVariantBuilder *service_properties_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(service_properties_builder, "{sv}", "UUID",
                              g_variant_new_string((char *) key));
        g_variant_builder_add(service_properties_builder, "{sv}", "Primary",
                              g_variant_new_boolean(TRUE));
        g_variant_builder_add(service_properties_builder, "{sv}", "Handle",
                      g_variant_new_uint16(localService->handle));
        g_variant_builder_add(service_properties_builder, "{sv}", "Characteristics",
                              binc_local_service_get_characteristics(localService));

        // Add the service to result
        g_variant_builder_add(service_builder, "{sa{sv}}", GATT_SERV_INTERFACE, service_properties_builder);
        g_variant_builder_unref(service_properties_builder);
        g_variant_builder_add(builder, "{oa{sa{sv}}}", localService->path, service_builder);
        g_variant_builder_unref(service_builder);
        add_characteristics(builder, localService);
    }
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

    if (g_str_equal(method, "GetManagedObjects")) {
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        if (application->services != NULL && g_hash_table_size(application->services) > 0) {
            add_services(application, builder);
        }
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
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(object_manager_xml, &error);
    if (error) {
        log_debug(TAG, "Unable to create manager node: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    application->registration_id = g_dbus_connection_register_object(application->connection,
                                                                     application->path,
                                                                     info->interfaces[0],
                                                                     &application_method_table,
                                                                     application,
                                                                     NULL,
                                                                     &error);
    g_dbus_node_info_unref(info);

    if (application->registration_id == 0 && error != NULL) {
        log_debug(TAG, "failed to publish application");
        g_clear_error(&error);
        return;
    }

    log_debug(TAG, "successfully published application");
}

Application *binc_create_application(const Adapter *adapter) {
    g_assert(adapter != NULL);
    char* random_str = random_string(4);

    Application *application = g_new0(Application, 1);
    application->connection = binc_adapter_get_dbus_connection(adapter);
    application->path = g_strdup_printf("/org/bluez/bincapp_%s_%s", binc_adapter_get_name(adapter), random_str);
    application->services = g_hash_table_new_full(g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  (GDestroyNotify) binc_local_service_free);

    binc_application_publish(application, adapter);

    g_free(random_str);
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

static GVariant *service_get_property(GDBusConnection *connection, const gchar *sender,
                                      const gchar *object_path, const gchar *interface_name,
                                      const gchar *property_name, GError **error, gpointer user_data) {
    LocalService *service = user_data;
    if (g_str_equal(property_name, "Handle")) {
        return g_variant_new_uint16(service->handle);
    } else if (g_str_equal(property_name, "UUID")) {
        return g_variant_new_string(service->uuid);
    } else if (g_str_equal(property_name, "Primary")) {
        return g_variant_new_boolean(TRUE);
    } else if (g_str_equal(property_name, "Characteristics")) {
        return binc_local_service_get_characteristics(service);
    }
    return NULL;
}

static gboolean service_set_property(GDBusConnection *connection, const gchar *sender,
                                      const gchar *object_path, const gchar *interface_name,
                                      const gchar *property_name, GVariant *value,
                                      GError **error, gpointer user_data) {
    LocalService *service = user_data;
    if (g_str_equal(property_name, "Handle")) {
        service->handle = g_variant_get_uint16(value);
        return TRUE;
    }
    return FALSE;
}

static const GDBusInterfaceVTable service_table = {
        .get_property = service_get_property,
        .set_property = service_set_property,
};

int binc_application_add_service(Application *application, const char *service_uuid) {
    g_return_val_if_fail (application != NULL, EINVAL);
    g_return_val_if_fail (is_valid_uuid(service_uuid), EINVAL);

    GError *error = NULL;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(service_xml, &error);
    if (error) {
        log_debug(TAG, "Unable to create service node: %s\n", error->message);
        g_clear_error(&error);
        return EINVAL;
    }

    LocalService *localService = g_new0(LocalService, 1);
    localService->uuid = g_strdup(service_uuid);
    localService->application = application;
    localService->characteristics = g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            (GDestroyNotify) binc_local_char_free);
    localService->path = g_strdup_printf(
            "%s/service%d",
            application->path,
            g_hash_table_size(application->services));
    g_hash_table_insert(application->services, g_strdup(service_uuid), localService);

    localService->registration_id = g_dbus_connection_register_object(application->connection,
                                                                      localService->path,
                                                                      info->interfaces[0],
                                                                      &service_table,
                                                                      localService,
                                                                      NULL,
                                                                      &error);
    g_dbus_node_info_unref(info);

    if (localService->registration_id == 0) {
        log_debug(TAG, "failed to publish local service");
        log_debug(TAG, "Error %s", error->message);
        g_hash_table_remove(application->services, service_uuid);
        binc_local_service_free(localService);
        g_clear_error(&error);
        return EINVAL;
    }

    log_debug(TAG, "successfully published local service %s", service_uuid);
    return 0;
}


static LocalService *binc_application_get_service(const Application *application, const char *service_uuid) {
    g_return_val_if_fail (application != NULL, NULL);
    g_return_val_if_fail (is_valid_uuid(service_uuid), NULL);

    return g_hash_table_lookup(application->services, service_uuid);
}

static GList *permissions2Flags(const guint permissions) {
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
    if (permissions & GATT_CHR_PROP_ENCRYPT_READ) {
        list = g_list_append(list, g_strdup("encrypt-read"));
    }
    if (permissions & GATT_CHR_PROP_ENCRYPT_WRITE) {
        list = g_list_append(list, g_strdup("encrypt-write"));
    }
    if (permissions & GATT_CHR_PROP_ENCRYPT_NOTIFY) {
        list = g_list_append(list, g_strdup("encrypt-notify"));
    }
    if (permissions & GATT_CHR_PROP_ENCRYPT_INDICATE) {
        list = g_list_append(list, g_strdup("encrypt-indicate"));
    }
    if (permissions & GATT_CHR_PROP_ENCRYPT_AUTH_READ) {
        list = g_list_append(list, g_strdup("encrypt-authenticated-read"));
    }
    if (permissions & GATT_CHR_PROP_ENCRYPT_AUTH_WRITE) {
        list = g_list_append(list, g_strdup("encrypt-authenticated-write"));
    }
    if (permissions & GATT_CHR_PROP_ENCRYPT_AUTH_NOTIFY) {
        list = g_list_append(list, g_strdup("encrypt-authenticated-notify"));
    }
    if (permissions & GATT_CHR_PROP_ENCRYPT_AUTH_INDICATE) {
        list = g_list_append(list, g_strdup("encrypt-authenticated-indicate"));
    }
    if (permissions & GATT_CHR_PROP_SECURE_READ) {
        list = g_list_append(list, g_strdup("secure-read"));
    }
    if (permissions & GATT_CHR_PROP_SECURE_WRITE) {
        list = g_list_append(list, g_strdup("secure-write"));
    }
    if (permissions & GATT_CHR_PROP_SECURE_NOTIFY) {
        list = g_list_append(list, g_strdup("secure-notify"));
    }
    if (permissions & GATT_CHR_PROP_SECURE_INDICATE) {
        list = g_list_append(list, g_strdup("secure-indicate"));
    }

    return list;
}

static int binc_characteristic_set_value(const Application *application, LocalCharacteristic *characteristic,
                                         GByteArray *byteArray) {
    g_return_val_if_fail (application != NULL, EINVAL);
    g_return_val_if_fail (characteristic != NULL, EINVAL);
    g_return_val_if_fail (byteArray != NULL, EINVAL);

    GString *byteArrayStr = g_byte_array_as_hex(byteArray);
    log_debug(TAG, "set value <%s> to <%s>", byteArrayStr->str, characteristic->uuid);
    g_string_free(byteArrayStr, TRUE);

    if (characteristic->value != NULL) {
        g_byte_array_free(characteristic->value, TRUE);
    }

    // Copy the byte array contents to the characteristic's value
    GByteArray *newByteArray = g_byte_array_sized_new(byteArray->len);
    g_byte_array_append(newByteArray, byteArray->data, byteArray->len);
    characteristic->value = newByteArray;

    if (application->on_char_updated != NULL) {
        application->on_char_updated(characteristic->application, characteristic->service_uuid,
                                     characteristic->uuid, characteristic->value);
    }

    return 0;
}

static int binc_descriptor_set_value(const Application *application, LocalDescriptor *descriptor,
                                     GByteArray *byteArray) {
    g_return_val_if_fail (application != NULL, EINVAL);
    g_return_val_if_fail (descriptor != NULL, EINVAL);
    g_return_val_if_fail (byteArray != NULL, EINVAL);

    GString *byteArrayStr = g_byte_array_as_hex(byteArray);
    log_debug(TAG, "set value <%s> to <%s>", byteArrayStr->str, descriptor->uuid);
    g_string_free(byteArrayStr, TRUE);

    if (descriptor->value != NULL) {
        g_byte_array_free(descriptor->value, TRUE);
    }

    // Copy the byte array contents to the descriptor's value
    GByteArray *newByteArray = g_byte_array_sized_new(byteArray->len);
    g_byte_array_append(newByteArray, byteArray->data, byteArray->len);
    descriptor->value = newByteArray;

//    if (application->on_char_updated != NULL) {
//        application->on_char_updated(characteristic->application, characteristic->service_uuid,
//                                     characteristic->uuid, byteArray);
//    }

    return 0;
}

static LocalCharacteristic *get_local_characteristic(const Application *application, const char *service_uuid,
                                                     const char *char_uuid) {

    g_return_val_if_fail (application != NULL, NULL);
    g_return_val_if_fail (is_valid_uuid(service_uuid), NULL);
    g_return_val_if_fail (is_valid_uuid(char_uuid), NULL);

    LocalService *service = binc_application_get_service(application, service_uuid);
    if (service != NULL) {
        return g_hash_table_lookup(service->characteristics, char_uuid);
    }
    return NULL;
}

static LocalDescriptor *get_local_descriptor(const Application *application, const char *service_uuid,
                                             const char *char_uuid, const char *desc_uuid) {

    g_return_val_if_fail (application != NULL, NULL);
    g_return_val_if_fail (is_valid_uuid(service_uuid), NULL);
    g_return_val_if_fail (is_valid_uuid(char_uuid), NULL);
    g_return_val_if_fail (is_valid_uuid(desc_uuid), NULL);

    LocalCharacteristic *characteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (characteristic != NULL) {
        return g_hash_table_lookup(characteristic->descriptors, desc_uuid);
    }
    return NULL;
}

int binc_application_set_service_handle(const Application *application, const char *service_uuid,
                                         guint16 handle) {
    LocalService *service = binc_application_get_service(application, service_uuid);
    if (service == NULL) return EINVAL;
    service->handle = handle;
    return 0;
}

int binc_application_set_char_handle(const Application *application, const char *service_uuid,
                                      const char *char_uuid, guint16 handle) {
    LocalCharacteristic *characteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (characteristic == NULL) return EINVAL;
    characteristic->handle = handle;
    return 0;
}

int binc_application_set_desc_handle(const Application *application, const char *service_uuid,
                                      const char *char_uuid, const char *desc_uuid, guint16 handle) {
    LocalDescriptor *descriptor = get_local_descriptor(application, service_uuid, char_uuid, desc_uuid);
    if (descriptor == NULL) return EINVAL;
    descriptor->handle = handle;
    return 0;
}

static void binc_internal_descriptor_method_call(GDBusConnection *conn,
                                                 const gchar *sender,
                                                 const gchar *path,
                                                 const gchar *interface,
                                                 const gchar *method,
                                                 GVariant *params,
                                                 GDBusMethodInvocation *invocation,
                                                 void *userdata) {

    LocalDescriptor *localDescriptor = (LocalDescriptor *) userdata;
    g_assert(localDescriptor != NULL);

    Application *application = localDescriptor->application;
    g_assert(application != NULL);

    if (g_str_equal(method, DESCRIPTOR_METHOD_READ_VALUE)) {
        ReadOptions *options = parse_read_options(params);

        log_debug(TAG, "read descriptor <%s> by %s", localDescriptor->uuid, options->device);

        const char *result = NULL;
        if (application->on_desc_read != NULL) {
            result = application->on_desc_read(localDescriptor->application, options->device,
                                               localDescriptor->service_uuid,
                                               localDescriptor->char_uuid, localDescriptor->uuid);
        }
        read_options_free(options);

        if (result) {
            g_dbus_method_invocation_return_dbus_error(invocation, result, "read descriptor error");
            log_debug(TAG, "read descriptor error");
            return;
        }

        if (localDescriptor->value != NULL) {
            GVariant *resultVariant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                                localDescriptor->value->data,
                                                                localDescriptor->value->len,
                                                                sizeof(guint8));
            g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&resultVariant, 1));
        } else {
            g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ_ERROR_FAILED, "no value for descriptor");
        }
    } else if (g_str_equal(method, DESCRIPTOR_METHOD_WRITE_VALUE)) {
        g_assert(g_str_equal(g_variant_get_type_string(params), "(aya{sv})"));
        GVariant *valueVariant, *optionsVariant;

        // Get the options
        g_variant_get(params, "(@ay@a{sv})", &valueVariant, &optionsVariant);
        WriteOptions *options = parse_write_options(optionsVariant);
        g_variant_unref(optionsVariant);

        // Get the byte array to be written
        GByteArray *byteArray = g_variant_get_byte_array(valueVariant);

        log_debug(TAG, "write descriptor <%s> by %s", localDescriptor->uuid, options->device);

        // Allow application to accept/reject the characteristic value before setting it
        const char *result = NULL;
        if (application->on_desc_write != NULL) {
            result = application->on_desc_write(localDescriptor->application,
                                                options->device,
                                                localDescriptor->service_uuid,
                                                localDescriptor->char_uuid,
                                                localDescriptor->uuid,
                                                byteArray);
        }
        write_options_free(options);

        if (result) {
            g_dbus_method_invocation_return_dbus_error(invocation, result, "write error");
            log_debug(TAG, "write error");
            g_variant_unref(valueVariant);
            if (byteArray != NULL) {
                g_byte_array_free(byteArray, FALSE);
            }
            return;
        }

        binc_descriptor_set_value(application, localDescriptor, byteArray);

        if (byteArray != NULL) {
            g_byte_array_free(byteArray, FALSE);
        }

        g_variant_unref(valueVariant);

        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    }
}

static GVariant *descriptor_get_property(GDBusConnection *connection, const gchar *sender,
                                         const gchar *object_path, const gchar *interface_name,
                                         const gchar *property_name, GError **error, gpointer user_data) {
    LocalDescriptor *descriptor = user_data;
    if (g_str_equal(property_name, "Handle")) {
        return g_variant_new_uint16(descriptor->handle);
    } else if (g_str_equal(property_name, "UUID")) {
        return g_variant_new_string(descriptor->uuid);
    } else if (g_str_equal(property_name, "Characteristic")) {
        return g_variant_new_object_path(descriptor->char_path);
    } else if (g_str_equal(property_name, "Flags")) {
        return binc_local_descriptor_get_flags(descriptor);
    } else if (g_str_equal(property_name, "Value")) {
        return g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                         descriptor->value ? descriptor->value->data : NULL,
                                         descriptor->value ? descriptor->value->len : 0,
                                         sizeof(guint8));
    }
    return NULL;
}

static gboolean descriptor_set_property(GDBusConnection *connection, const gchar *sender,
                                         const gchar *object_path, const gchar *interface_name,
                                         const gchar *property_name, GVariant *value,
                                         GError **error, gpointer user_data) {
    LocalDescriptor *descriptor = user_data;
    if (g_str_equal(property_name, "Handle")) {
        descriptor->handle = g_variant_get_uint16(value);
        return TRUE;
    }
    return FALSE;
}

static const GDBusInterfaceVTable descriptor_table = {
        .method_call = binc_internal_descriptor_method_call,
        .get_property = descriptor_get_property,
        .set_property = descriptor_set_property,
};

int binc_application_add_descriptor(Application *application, const char *service_uuid,
                                    const char *char_uuid, const char *desc_uuid, guint permissions) {
    g_return_val_if_fail (application != NULL, EINVAL);
    g_return_val_if_fail (is_valid_uuid(service_uuid), EINVAL);

    LocalCharacteristic *localCharacteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (localCharacteristic == NULL) {
        g_critical("characteristic %s does not exist", char_uuid);
        return EINVAL;
    }

    GError *error = NULL;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(descriptor_xml, &error);
    if (error) {
        log_debug(TAG, "Unable to create descriptor node: %s\n", error->message);
        g_clear_error(&error);
        return EINVAL;
    }

    LocalDescriptor *localDescriptor = g_new0(LocalDescriptor, 1);
    localDescriptor->uuid = g_strdup(desc_uuid);
    localDescriptor->application = application;
    localDescriptor->char_path = g_strdup(localCharacteristic->path);
    localDescriptor->char_uuid = g_strdup(char_uuid);
    localDescriptor->service_uuid = g_strdup(service_uuid);
    localDescriptor->flags = permissions2Flags(permissions);
    localDescriptor->path = g_strdup_printf("%s/desc%d",
                                            localCharacteristic->path,
                                            g_hash_table_size(localCharacteristic->descriptors));
    g_hash_table_insert(localCharacteristic->descriptors, g_strdup(desc_uuid), localDescriptor);

    // Register characteristic
    localDescriptor->registration_id = g_dbus_connection_register_object(application->connection,
                                                                         localDescriptor->path,
                                                                         info->interfaces[0],
                                                                         &descriptor_table,
                                                                         localDescriptor,
                                                                         NULL,
                                                                         &error);
    g_dbus_node_info_unref(info);

    if (localDescriptor->registration_id == 0) {
        log_debug(TAG, "failed to publish local characteristic");
        log_debug(TAG, "Error %s", error->message);
        g_clear_error(&error);
        g_hash_table_remove(localCharacteristic->descriptors, desc_uuid);
        return EINVAL;
    }

    log_debug(TAG, "successfully published local descriptor %s", desc_uuid);
    return 0;
}

int binc_application_set_char_value(const Application *application, const char *service_uuid,
                                    const char *char_uuid, GByteArray *byteArray) {

    g_return_val_if_fail (application != NULL, EINVAL);
    g_return_val_if_fail (service_uuid != NULL, EINVAL);
    g_return_val_if_fail (char_uuid != NULL, EINVAL);
    g_return_val_if_fail (byteArray != NULL, EINVAL);
    g_return_val_if_fail (is_valid_uuid(service_uuid), EINVAL);
    g_return_val_if_fail (is_valid_uuid(char_uuid), EINVAL);

    LocalCharacteristic *characteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (characteristic == NULL) {
        g_critical("%s: characteristic with uuid %s does not exist", G_STRFUNC, char_uuid);
        return EINVAL;
    }

    return binc_characteristic_set_value(application, characteristic, byteArray);
}

int binc_application_set_desc_value(const Application *application, const char *service_uuid,
                                    const char *char_uuid, const char *desc_uuid, GByteArray *byteArray) {

    g_return_val_if_fail (application != NULL, EINVAL);
    g_return_val_if_fail (service_uuid != NULL, EINVAL);
    g_return_val_if_fail (char_uuid != NULL, EINVAL);
    g_return_val_if_fail (byteArray != NULL, EINVAL);
    g_return_val_if_fail (is_valid_uuid(service_uuid), EINVAL);
    g_return_val_if_fail (is_valid_uuid(char_uuid), EINVAL);

    LocalDescriptor *descriptor = get_local_descriptor(application, service_uuid, char_uuid, desc_uuid);
    if (descriptor == NULL) {
        g_critical("%s: characteristic with uuid %s does not exist", G_STRFUNC, char_uuid);
        return EINVAL;
    }

    return binc_descriptor_set_value(application, descriptor, byteArray);
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


static gboolean notify_client_closed(gint fd, GIOCondition condition, gpointer user_data) {
    LocalNotifyClient *client = user_data;
    LocalCharacteristic *characteristic = client->characteristic;
    char *address = g_strdup(client->address);
    guint16 mtu = client->mtu;
    g_mutex_lock(&characteristic->notify_mutex);
    g_hash_table_remove(characteristic->notify_clients, address);
    characteristic->notifying = g_hash_table_size(characteristic->notify_clients) > 0;
    g_mutex_unlock(&characteristic->notify_mutex);
    if (characteristic->application->on_char_notify_state != NULL) {
        characteristic->application->on_char_notify_state(characteristic->application, address,
                characteristic->service_uuid, characteristic->uuid, FALSE, mtu);
    }
    g_free(address);
    return G_SOURCE_REMOVE;
}

static void acquire_notify(LocalCharacteristic *characteristic, GVariant *params,
                            GDBusMethodInvocation *invocation) {
    if (!(characteristic->permissions & GATT_CHR_PROP_NOTIFY) ||
            characteristic->application->on_char_notify_state == NULL) {
        g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ_ERROR_NOT_SUPPORTED,
                                                   "Per-device notifications are not enabled");
        return;
    }
    ReadOptions *options = parse_read_options(params);
    if (options->device == NULL || options->mtu < 23) {
        read_options_free(options);
        g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ_ERROR_REJECTED,
                                                   "A device and valid MTU are required");
        return;
    }
    char *address = g_ascii_strup(options->device, -1);
    guint16 mtu = options->mtu;
    read_options_free(options);
    g_mutex_lock(&characteristic->notify_mutex);
    if (g_hash_table_contains(characteristic->notify_clients, address)) {
        g_mutex_unlock(&characteristic->notify_mutex);
        g_free(address);
        g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ_ERROR_INPROGRESS,
                                                   "Notifications already acquired by this device");
        return;
    }
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, sockets) != 0) {
        g_mutex_unlock(&characteristic->notify_mutex);
        g_free(address);
        g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ_ERROR_FAILED,
                                                   "Unable to create notification socket");
        return;
    }
    LocalNotifyClient *client = g_new0(LocalNotifyClient, 1);
    client->address = address;
    client->characteristic = characteristic;
    client->fd = sockets[0];
    client->mtu = mtu;
    client->source = g_unix_fd_source_new(client->fd, G_IO_HUP | G_IO_ERR | G_IO_NVAL);
    g_source_set_callback(client->source, G_SOURCE_FUNC(notify_client_closed), client, NULL);
    g_source_attach(client->source, g_main_context_get_thread_default());
    g_hash_table_insert(characteristic->notify_clients, g_strdup(address), client);
    characteristic->notifying = TRUE;
    g_mutex_unlock(&characteristic->notify_mutex);
    GUnixFDList *fd_list = g_unix_fd_list_new_from_array(&sockets[1], 1);
    g_dbus_method_invocation_return_value_with_unix_fd_list(invocation,
                                                            g_variant_new("(hq)", 0, mtu), fd_list);
    g_object_unref(fd_list);
    characteristic->application->on_char_notify_state(characteristic->application, address,
            characteristic->service_uuid, characteristic->uuid, TRUE, mtu);
}

static void binc_internal_characteristic_method_call(GDBusConnection *conn,
                                                     const gchar *sender,
                                                     const gchar *path,
                                                     const gchar *interface,
                                                     const gchar *method,
                                                     GVariant *params,
                                                     GDBusMethodInvocation *invocation,
                                                     void *userdata) {

    LocalCharacteristic *characteristic = (LocalCharacteristic *) userdata;
    g_assert(characteristic != NULL);

    Application *application = characteristic->application;
    g_assert(application != NULL);

    if (g_str_equal(method, "AcquireNotify")) {
        acquire_notify(characteristic, params, invocation);
    } else if (g_str_equal(method, CHARACTERISTIC_METHOD_READ_VALUE)) {
        log_debug(TAG, "read <%s>", characteristic->uuid);
        ReadOptions *options = parse_read_options(params);

        // Allow application to accept/reject the characteristic value before setting it
        const char *result = NULL;
        if (application->on_char_read != NULL) {
            result = application->on_char_read(characteristic->application, options->device,
                                               characteristic->service_uuid,
                                               characteristic->uuid, options->mtu, options->offset);
        }
        read_options_free(options);

        if (result) {
            g_dbus_method_invocation_return_dbus_error(invocation, result, "read characteristic error");
            log_debug(TAG, "read characteristic error '%s'", result);
            return;
        }

        // TODO deal with the offset & mtu parameter
        if (characteristic->value != NULL) {
            GVariant *resultVariant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                                characteristic->value->data,
                                                                characteristic->value->len,
                                                                sizeof(guint8));
            g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&resultVariant, 1));
        } else {
            g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ_ERROR_FAILED, "no value");
        }
    } else if (g_str_equal(method, CHARACTERISTIC_METHOD_WRITE_VALUE)) {
        g_assert(g_str_equal(g_variant_get_type_string(params), "(aya{sv})"));
        GVariant *valueVariant, *optionsVariant;

        // Get the write options
        g_variant_get(params, "(@ay@a{sv})", &valueVariant, &optionsVariant);
        WriteOptions *options = parse_write_options(optionsVariant);
        g_variant_unref(optionsVariant);

        // Get the byte array to be written
        GByteArray *byteArray = g_variant_get_byte_array(valueVariant);

        log_debug(TAG, "write <%s>", characteristic->uuid);

        // Allow application to accept/reject the characteristic value before setting it
        const char *result = NULL;
        if (application->on_char_write != NULL) {
            result = application->on_char_write(characteristic->application, options->device,
                                                characteristic->service_uuid,
                                                characteristic->uuid, byteArray, options->mtu, options->offset);
        }
        write_options_free(options);

        if (result) {
            g_dbus_method_invocation_return_dbus_error(invocation, result, "write error");
            log_debug(TAG, "write error");
            return;
        }

        // TODO deal with offset and mtu
        binc_characteristic_set_value(application, characteristic, byteArray);

        if (byteArray != NULL) {
            g_byte_array_free(byteArray, FALSE);
        }

        g_variant_unref(valueVariant);

        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (g_str_equal(method, CHARACTERISTIC_METHOD_START_NOTIFY)) {
        log_debug(TAG, "start notify <%s>", characteristic->uuid);

        characteristic->notifying = TRUE;
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));

        if (application->on_char_start_notify != NULL) {
            application->on_char_start_notify(characteristic->application, characteristic->service_uuid,
                                              characteristic->uuid);
        }
    } else if (g_str_equal(method, CHARACTERISTIC_METHOD_STOP_NOTIFY)) {
        log_debug(TAG, "stop notify <%s>", characteristic->uuid);

        characteristic->notifying = FALSE;
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));

        if (application->on_char_stop_notify != NULL) {
            application->on_char_stop_notify(characteristic->application, characteristic->service_uuid,
                                             characteristic->uuid);
        }
    } else if (g_str_equal(method, CHARACTERISTIC_METHOD_CONFIRM)) {
        log_debug(TAG, "indication confirmed <%s>", characteristic->uuid);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
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

    GVariant *ret = NULL;
    if (g_str_equal(property_name, "Handle")) {
        ret = g_variant_new_uint16(characteristic->handle);
    } else if (g_str_equal(property_name, "UUID")) {
        ret = g_variant_new_string(characteristic->uuid);
    } else if (g_str_equal(property_name, "Service")) {
        ret = g_variant_new_object_path(characteristic->service_path);
    } else if (g_str_equal(property_name, "Flags")) {
        ret = binc_local_characteristic_get_flags(characteristic);
    } else if (g_str_equal(property_name, "Notifying")) {
        ret = g_variant_new_boolean(characteristic->notifying);
    } else if (g_str_equal(property_name, "NotifyAcquired")) {
        if (!(characteristic->permissions & GATT_CHR_PROP_NOTIFY) ||
                characteristic->application->on_char_notify_state == NULL) {
            g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
                                 "Per-device notifications are not enabled");
            return NULL;
        }
        g_mutex_lock(&characteristic->notify_mutex);
        ret = g_variant_new_boolean(g_hash_table_size(characteristic->notify_clients) > 0);
        g_mutex_unlock(&characteristic->notify_mutex);
    } else if (g_str_equal(property_name, "Value")) {
        ret = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                        characteristic->value ? characteristic->value->data : NULL,
                                        characteristic->value ? characteristic->value->len : 0,
                                        sizeof(guint8));
    } else if (g_str_equal(property_name, "Descriptors")) {
        ret = binc_local_characteristic_get_descriptors(characteristic);
    }
    return ret;
}

static gboolean characteristic_set_property(GDBusConnection *connection, const gchar *sender,
                                             const gchar *object_path, const gchar *interface_name,
                                             const gchar *property_name, GVariant *value,
                                             GError **error, gpointer user_data) {
    LocalCharacteristic *characteristic = user_data;
    if (g_str_equal(property_name, "Handle")) {
        characteristic->handle = g_variant_get_uint16(value);
        return TRUE;
    }
    return FALSE;
}

static const GDBusInterfaceVTable characteristic_table = {
        .method_call = binc_internal_characteristic_method_call,
        .get_property = characteristic_get_property,
        .set_property = characteristic_set_property,
};

int binc_application_add_characteristic(Application *application, const char *service_uuid,
                                        const char *char_uuid, guint permissions) {

    g_return_val_if_fail (application != NULL, EINVAL);
    g_return_val_if_fail (is_valid_uuid(service_uuid), EINVAL);
    g_return_val_if_fail (is_valid_uuid(char_uuid), EINVAL);

    LocalService *localService = binc_application_get_service(application, service_uuid);
    if (localService == NULL) {
        g_critical("service %s does not exist", service_uuid);
        return EINVAL;
    }

    GError *error = NULL;
    GDBusNodeInfo *info = NULL;
    info = g_dbus_node_info_new_for_xml(characteristic_xml, &error);
    if (error) {
        log_debug(TAG, "Unable to create node: %s\n", error->message);
        g_clear_error(&error);
        return EINVAL;
    }

    LocalCharacteristic *characteristic = g_new0(LocalCharacteristic, 1);
    characteristic->service_uuid = g_strdup(service_uuid);
    characteristic->service_path = g_strdup(localService->path);
    characteristic->uuid = g_strdup(char_uuid);
    characteristic->permissions = permissions;
    characteristic->flags = permissions2Flags(permissions);
    characteristic->value = NULL;
    characteristic->application = application;
    characteristic->path = g_strdup_printf("%s/char%d",
                                           localService->path,
                                           g_hash_table_size(localService->characteristics));
    characteristic->descriptors = g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            (GDestroyNotify) binc_local_desc_free);
    g_mutex_init(&characteristic->notify_mutex);
    characteristic->notify_clients = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                                            (GDestroyNotify)notify_client_free);
    g_hash_table_insert(localService->characteristics, g_strdup(char_uuid), characteristic);

    // Register characteristic
    characteristic->registration_id = g_dbus_connection_register_object(application->connection,
                                                                        characteristic->path,
                                                                        info->interfaces[0],
                                                                        &characteristic_table,
                                                                        characteristic,
                                                                        NULL,
                                                                        &error);
    g_dbus_node_info_unref(info);

    if (characteristic->registration_id == 0) {
        log_debug(TAG, "failed to publish local characteristic");
        log_debug(TAG, "Error %s", error->message);
        g_clear_error(&error);
        g_hash_table_remove(localService->characteristics, char_uuid);
        return EINVAL;
    }

    log_debug(TAG, "successfully published local characteristic %s", char_uuid);
    return 0;
}

const char *binc_application_get_path(const Application *application) {
    g_assert(application != NULL);
    return application->path;
}

void binc_application_set_char_read_cb(Application *application, onLocalCharacteristicRead callback) {
    g_assert(application != NULL);
    g_assert(callback != NULL);

    application->on_char_read = callback;
}

void binc_application_set_char_updated_cb(Application *application, onLocalCharacteristicUpdated callback) {
    g_assert(application != NULL);
    g_assert(callback != NULL);

    application->on_char_updated = callback;
}

void binc_application_set_char_write_cb(Application *application, onLocalCharacteristicWrite callback) {
    g_assert(application != NULL);
    g_assert(callback != NULL);

    application->on_char_write = callback;
}

void binc_application_set_desc_read_cb(Application *application, onLocalDescriptorRead callback) {
    g_assert(application != NULL);
    g_assert(callback != NULL);

    application->on_desc_read = callback;
}

void binc_application_set_desc_write_cb(Application *application, onLocalDescriptorWrite callback) {
    g_assert(application != NULL);
    g_assert(callback != NULL);

    application->on_desc_write = callback;
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

void binc_application_set_char_notify_state_cb(Application *application, onLocalCharacteristicNotifyState callback) {
    g_assert(application != NULL);
    application->on_char_notify_state = callback;
}

int binc_application_notify_device(const Application *application, const char *address,
                                    const char *service_uuid, const char *char_uuid,
                                    const GByteArray *byteArray) {
    g_return_val_if_fail(address != NULL, EINVAL);
    g_return_val_if_fail(byteArray != NULL, EINVAL);
    LocalCharacteristic *characteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (characteristic == NULL) return EINVAL;
    char *key = g_ascii_strup(address, -1);
    g_mutex_lock(&characteristic->notify_mutex);
    LocalNotifyClient *client = g_hash_table_lookup(characteristic->notify_clients, key);
    int result = ENOTCONN;
    if (client != NULL) {
        if (byteArray->len > (guint)(client->mtu - 3)) {
            result = EMSGSIZE;
        } else {
            ssize_t sent = send(client->fd, byteArray->data, byteArray->len, MSG_NOSIGNAL);
            result = sent < 0 ? errno : ((gsize)sent == byteArray->len ? 0 : EIO);
        }
    }
    g_mutex_unlock(&characteristic->notify_mutex);
    g_free(key);
    return result;
}

int binc_application_notify(const Application *application, const char *service_uuid, const char *char_uuid,
                            const GByteArray *byteArray) {

    g_return_val_if_fail (application != NULL, EINVAL);
    g_return_val_if_fail (byteArray != NULL, EINVAL);
    g_return_val_if_fail (is_valid_uuid(service_uuid), EINVAL);
    g_return_val_if_fail (is_valid_uuid(char_uuid), EINVAL);

    LocalCharacteristic *characteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (characteristic == NULL) {
        g_critical("%s: characteristic %s does not exist", G_STRFUNC, service_uuid);
        return EINVAL;
    }

    if (application->on_char_notify_state != NULL) return ENOTSUP;

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
                                                    g_variant_new("(sa{sv}as)",
                                                                  "org.bluez.GattCharacteristic1",
                                                                  properties_builder, invalidated_builder),
                                                    &error);

    g_variant_builder_unref(invalidated_builder);
    g_variant_builder_unref(properties_builder);

    if (result != TRUE) {
        if (error != NULL) {
            log_debug(TAG, "error emitting signal: %s", error->message);
            g_clear_error(&error);
        }
        return EINVAL;
    }

    GString *byteArrayStr = g_byte_array_as_hex(byteArray);
    log_debug(TAG, "notified <%s> on <%s>", byteArrayStr->str, characteristic->uuid);
    g_string_free(byteArrayStr, TRUE);
    return 0;
}

gboolean binc_application_char_is_notifying(const Application *application, const char *service_uuid,
                                            const char *char_uuid) {
    g_return_val_if_fail (application != NULL, FALSE);
    g_return_val_if_fail (is_valid_uuid(service_uuid), FALSE);
    g_return_val_if_fail (is_valid_uuid(char_uuid), FALSE);

    LocalCharacteristic *characteristic = get_local_characteristic(application, service_uuid, char_uuid);
    if (characteristic == NULL) {
        g_critical("%s: characteristic %s does not exist", G_STRFUNC, service_uuid);
        return FALSE;
    }

    return characteristic->notifying;
}

void binc_application_set_user_data(Application *application, void *user_data){
    g_assert(application != NULL);
    application->user_data = user_data;
}

void *binc_application_get_user_data(const Application *application){
    g_assert(application != NULL);
    return application->user_data;
}
