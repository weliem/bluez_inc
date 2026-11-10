#include "../binc/application.c"

#define TEST_SERVICE_UUID "0000180a-0000-1000-8000-00805f9b34fb"
#define TEST_CHAR_UUID "00002a29-0000-1000-8000-00805f9b34fb"
#define TEST_DESC_UUID "00002901-0000-1000-8000-00805f9b34fb"
#define MISSING_UUID "0000180f-0000-1000-8000-00805f9b34fb"

static GDBusConnection *test_connection;

typedef struct {
    gboolean done;
    GVariant *reply;
    GError *error;
    GUnixFDList *fds;
} TestCall;

static void call_finished(GObject *source, GAsyncResult *result, gpointer user_data) {
    TestCall *call = user_data;
    call->reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &call->error);
    call->done = TRUE;
}

static GVariant *call_method(const char *path, const char *interface, const char *method,
                             GVariant *parameters, const GVariantType *reply_type) {
    TestCall call = {0};
    g_dbus_connection_call(test_connection, g_dbus_connection_get_unique_name(test_connection),
                           path, interface, method, parameters, reply_type,
                           G_DBUS_CALL_FLAGS_NONE, 3000, NULL, call_finished, &call);
    while (!call.done) {
        g_main_context_iteration(NULL, TRUE);
    }
    g_assert_no_error(call.error);
    g_assert_nonnull(call.reply);
    return call.reply;
}

static Application *create_test_application(void) {
    Application *application = g_new0(Application, 1);
    application->connection = test_connection;
    application->path = g_strdup("/org/bluez/binc_handle_test");
    application->services = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                                   (GDestroyNotify)binc_local_service_free);

    GError *error = NULL;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(object_manager_xml, &error);
    g_assert_no_error(error);
    application->registration_id = g_dbus_connection_register_object(
            test_connection, application->path, info->interfaces[0],
            &application_method_table, application, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(application->registration_id, !=, 0);
    g_dbus_node_info_unref(info);

    g_assert_cmpint(binc_application_add_service(application, TEST_SERVICE_UUID), ==, 0);
    g_assert_cmpint(binc_application_add_characteristic(application, TEST_SERVICE_UUID,
                     TEST_CHAR_UUID, GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY), ==, 0);
    g_assert_cmpint(binc_application_add_descriptor(application, TEST_SERVICE_UUID,
                     TEST_CHAR_UUID, TEST_DESC_UUID, GATT_CHR_PROP_READ), ==, 0);
    return application;
}

static void assert_property_handle(const char *path, const char *interface, guint16 expected) {
    GVariant *reply = call_method(path, "org.freedesktop.DBus.Properties", "Get",
                                   g_variant_new("(ss)", interface, "Handle"), G_VARIANT_TYPE("(v)"));
    GVariant *value = NULL;
    g_variant_get(reply, "(v)", &value);
    g_assert_true(g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16));
    g_assert_cmpuint(g_variant_get_uint16(value), ==, expected);
    g_variant_unref(value);
    g_variant_unref(reply);
}

static void assert_managed_handle(GVariant *objects, const char *path,
                                   const char *interface, guint16 expected) {
    GVariant *interfaces = g_variant_lookup_value(objects, path, G_VARIANT_TYPE("a{sa{sv}}"));
    g_assert_nonnull(interfaces);
    GVariant *properties = g_variant_lookup_value(interfaces, interface, G_VARIANT_TYPE("a{sv}"));
    g_assert_nonnull(properties);
    guint16 handle = 0;
    g_assert_true(g_variant_lookup(properties, "Handle", "q", &handle));
    g_assert_cmpuint(handle, ==, expected);
    g_variant_unref(properties);
    g_variant_unref(interfaces);
}

static void assert_handles(Application *application, guint16 service_handle,
                            guint16 char_handle, guint16 desc_handle) {
    LocalService *service = binc_application_get_service(application, TEST_SERVICE_UUID);
    LocalCharacteristic *characteristic = get_local_characteristic(application, TEST_SERVICE_UUID,
                                                                    TEST_CHAR_UUID);
    LocalDescriptor *descriptor = get_local_descriptor(application, TEST_SERVICE_UUID,
                                                        TEST_CHAR_UUID, TEST_DESC_UUID);
    GVariant *reply = call_method(application->path, "org.freedesktop.DBus.ObjectManager",
                                   "GetManagedObjects", NULL, G_VARIANT_TYPE("(a{oa{sa{sv}}})"));
    GVariant *objects = g_variant_get_child_value(reply, 0);
    assert_managed_handle(objects, service->path, GATT_SERV_INTERFACE, service_handle);
    assert_managed_handle(objects, characteristic->path, GATT_CHAR_INTERFACE, char_handle);
    assert_managed_handle(objects, descriptor->path, GATT_DESC_INTERFACE, desc_handle);
    g_variant_unref(objects);
    g_variant_unref(reply);

    assert_property_handle(service->path, GATT_SERV_INTERFACE, service_handle);
    assert_property_handle(characteristic->path, GATT_CHAR_INTERFACE, char_handle);
    assert_property_handle(descriptor->path, GATT_DESC_INTERFACE, desc_handle);

    const char *paths[] = {service->path, characteristic->path, descriptor->path};
    const char *interfaces[] = {GATT_SERV_INTERFACE, GATT_CHAR_INTERFACE, GATT_DESC_INTERFACE};
    for (guint index = 0; index < G_N_ELEMENTS(paths); index++) {
        reply = call_method(paths[index], "org.freedesktop.DBus.Properties", "GetAll",
                              g_variant_new("(s)", interfaces[index]), G_VARIANT_TYPE("(a{sv})"));
        g_variant_unref(reply);
    }
}

static void test_default_handles(void) {
    Application *application = create_test_application();
    assert_handles(application, 0, 0, 0);
    binc_application_free(application);
}

static void test_fixed_handles(void) {
    for (guint attempt = 0; attempt < 3; attempt++) {
        Application *application = create_test_application();
        g_assert_cmpint(binc_application_set_service_handle(application, TEST_SERVICE_UUID, 0x0100), ==, 0);
        g_assert_cmpint(binc_application_set_char_handle(application, TEST_SERVICE_UUID,
                         TEST_CHAR_UUID, 0x0102), ==, 0);
        g_assert_cmpint(binc_application_set_desc_handle(application, TEST_SERVICE_UUID,
                         TEST_CHAR_UUID, TEST_DESC_UUID, 0x0104), ==, 0);
        assert_handles(application, 0x0100, 0x0102, 0x0104);
        binc_application_free(application);
    }
}

static void test_handle_writeback(void) {
    Application *application = create_test_application();
    LocalService *service = binc_application_get_service(application, TEST_SERVICE_UUID);
    LocalCharacteristic *characteristic = get_local_characteristic(application, TEST_SERVICE_UUID,
                                                                    TEST_CHAR_UUID);
    LocalDescriptor *descriptor = get_local_descriptor(application, TEST_SERVICE_UUID,
                                                        TEST_CHAR_UUID, TEST_DESC_UUID);
    const char *paths[] = {service->path, characteristic->path, descriptor->path};
    const char *interfaces[] = {GATT_SERV_INTERFACE, GATT_CHAR_INTERFACE, GATT_DESC_INTERFACE};
    const guint16 handles[] = {1, 0xfffd, 0xffff};
    for (guint index = 0; index < G_N_ELEMENTS(paths); index++) {
        GVariant *reply = call_method(paths[index], "org.freedesktop.DBus.Properties", "Set",
                                       g_variant_new("(ssv)", interfaces[index], "Handle",
                                                     g_variant_new_uint16(handles[index])),
                                       G_VARIANT_TYPE("()"));
        g_variant_unref(reply);
    }
    assert_handles(application, handles[0], handles[1], handles[2]);
    g_assert_cmpint(binc_application_set_service_handle(application, TEST_SERVICE_UUID, 0), ==, 0);
    g_assert_cmpint(binc_application_set_char_handle(application, TEST_SERVICE_UUID,
                     TEST_CHAR_UUID, 0), ==, 0);
    g_assert_cmpint(binc_application_set_desc_handle(application, TEST_SERVICE_UUID,
                     TEST_CHAR_UUID, TEST_DESC_UUID, 0), ==, 0);
    assert_handles(application, 0, 0, 0);
    binc_application_free(application);
}

static void test_missing_objects(void) {
    Application *application = create_test_application();
    g_assert_cmpint(binc_application_set_service_handle(application, MISSING_UUID, 0x0100), ==, EINVAL);
    g_assert_cmpint(binc_application_set_char_handle(application, MISSING_UUID,
                     TEST_CHAR_UUID, 0x0102), ==, EINVAL);
    g_assert_cmpint(binc_application_set_char_handle(application, TEST_SERVICE_UUID,
                     MISSING_UUID, 0x0102), ==, EINVAL);
    g_assert_cmpint(binc_application_set_desc_handle(application, TEST_SERVICE_UUID,
                     MISSING_UUID, TEST_DESC_UUID, 0x0104), ==, EINVAL);
    g_assert_cmpint(binc_application_set_desc_handle(application, TEST_SERVICE_UUID,
                     TEST_CHAR_UUID, MISSING_UUID, 0x0104), ==, EINVAL);
    assert_handles(application, 0, 0, 0);
    binc_application_free(application);
}

static void notify_state_changed(const Application *application, const char *address,
                                  const char *service_uuid, const char *char_uuid,
                                  gboolean notifying, guint16 mtu) {
    g_assert_nonnull(address);
    g_assert_cmpuint(mtu, >=, 23);
}

static void acquire_finished(GObject *source, GAsyncResult *result, gpointer user_data) {
    TestCall *call = user_data;
    call->reply = g_dbus_connection_call_with_unix_fd_list_finish(G_DBUS_CONNECTION(source),
                                                                  &call->fds, result, &call->error);
    call->done = TRUE;
}

static TestCall try_acquire_client(LocalCharacteristic *characteristic, const char *device, guint16 mtu) {
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
    if (device != NULL) {
        g_variant_builder_add(&options, "{sv}", "device", g_variant_new_object_path(device));
    }
    g_variant_builder_add(&options, "{sv}", "mtu", g_variant_new_uint16(mtu));
    TestCall call = {0};
    g_dbus_connection_call_with_unix_fd_list(test_connection,
            g_dbus_connection_get_unique_name(test_connection), characteristic->path,
            GATT_CHAR_INTERFACE, "AcquireNotify", g_variant_new("(a{sv})", &options),
            G_VARIANT_TYPE("(hq)"), G_DBUS_CALL_FLAGS_NONE, 3000, NULL, NULL, acquire_finished, &call);
    while (!call.done) g_main_context_iteration(NULL, TRUE);
    return call;
}

static int acquire_client(LocalCharacteristic *characteristic, const char *device, guint16 mtu) {
    TestCall call = try_acquire_client(characteristic, device, mtu);
    g_assert_no_error(call.error);
    g_assert_nonnull(call.reply);
    gint index;
    guint16 returned_mtu;
    g_variant_get(call.reply, "(hq)", &index, &returned_mtu);
    g_assert_cmpuint(returned_mtu, ==, mtu);
    int fd = g_unix_fd_list_get(call.fds, index, &call.error);
    g_assert_no_error(call.error);
    g_variant_unref(call.reply);
    g_object_unref(call.fds);
    return fd;
}

static void test_notification_isolation(void) {
    Application *application = create_test_application();
    binc_application_set_char_notify_state_cb(application, notify_state_changed);
    LocalCharacteristic *characteristic = get_local_characteristic(application, TEST_SERVICE_UUID,
                                                                    TEST_CHAR_UUID);
    int first = acquire_client(characteristic, "/org/bluez/hci0/dev_00_11_22_33_44_55", 23);
    int second = acquire_client(characteristic, "/org/bluez/hci0/dev_00_11_22_33_44_66", 50);
    GByteArray *payload = g_byte_array_new();
    g_byte_array_append(payload, (const guint8 *)"first", 5);
    g_assert_cmpint(binc_application_notify_device(application, "00:11:22:33:44:55",
                     TEST_SERVICE_UUID, TEST_CHAR_UUID, payload), ==, 0);
    char received[64];
    g_assert_cmpint(recv(first, received, sizeof(received), MSG_DONTWAIT), ==, 5);
    g_assert_cmpmem(received, 5, "first", 5);
    g_assert_cmpint(recv(second, received, sizeof(received), MSG_DONTWAIT), ==, -1);
    g_assert_cmpint(errno, ==, EAGAIN);
    g_assert_cmpint(binc_application_notify(application, TEST_SERVICE_UUID, TEST_CHAR_UUID, payload), ==, ENOTSUP);
    g_byte_array_set_size(payload, 21);
    g_assert_cmpint(binc_application_notify_device(application, "00:11:22:33:44:55",
                     TEST_SERVICE_UUID, TEST_CHAR_UUID, payload), ==, EMSGSIZE);
    close(first);
    while (g_hash_table_size(characteristic->notify_clients) != 1) {
        g_main_context_iteration(NULL, TRUE);
    }
    g_assert_cmpint(binc_application_notify_device(application, "00:11:22:33:44:55",
                     TEST_SERVICE_UUID, TEST_CHAR_UUID, payload), ==, ENOTCONN);
    g_byte_array_set_size(payload, 0);
    g_byte_array_append(payload, (const guint8 *)"second", 6);
    g_assert_cmpint(binc_application_notify_device(application, "00:11:22:33:44:66",
                     TEST_SERVICE_UUID, TEST_CHAR_UUID, payload), ==, 0);
    g_assert_cmpint(recv(second, received, sizeof(received), MSG_DONTWAIT), ==, 6);
    g_assert_cmpmem(received, 6, "second", 6);
    g_byte_array_unref(payload);
    binc_application_free(application);
    g_assert_cmpint(recv(second, received, sizeof(received), MSG_DONTWAIT), ==, 0);
    close(second);
}

static void assert_acquire_error(LocalCharacteristic *characteristic, const char *device,
                                  guint16 mtu, const char *expected) {
    guint count = g_hash_table_size(characteristic->notify_clients);
    TestCall call = try_acquire_client(characteristic, device, mtu);
    g_assert_null(call.reply);
    g_assert_nonnull(call.error);
    char *remote = g_dbus_error_get_remote_error(call.error);
    g_assert_cmpstr(remote, ==, expected);
    g_free(remote);
    g_clear_error(&call.error);
    g_clear_object(&call.fds);
    g_assert_cmpuint(g_hash_table_size(characteristic->notify_clients), ==, count);
}

static void test_notification_rejection(void) {
    Application *application = create_test_application();
    LocalCharacteristic *characteristic = get_local_characteristic(application, TEST_SERVICE_UUID,
                                                                    TEST_CHAR_UUID);
    const char *device = "/org/bluez/hci0/dev_00_11_22_33_44_55";
    assert_acquire_error(characteristic, device, 23, BLUEZ_ERROR_NOT_SUPPORTED);
    binc_application_set_char_notify_state_cb(application, notify_state_changed);
    assert_acquire_error(characteristic, NULL, 23, BLUEZ_ERROR_REJECTED);
    assert_acquire_error(characteristic, device, 0, BLUEZ_ERROR_REJECTED);
    assert_acquire_error(characteristic, device, 22, BLUEZ_ERROR_REJECTED);
    int first = acquire_client(characteristic, device, 23);
    assert_acquire_error(characteristic, device, 23, BLUEZ_ERROR_INPROGRESS);
    int second = acquire_client(characteristic, "/org/bluez/hci0/dev_00_11_22_33_44_66", 50);
    binc_application_free(application);
    char value;
    g_assert_cmpint(recv(first, &value, sizeof(value), MSG_DONTWAIT), ==, 0);
    g_assert_cmpint(recv(second, &value, sizeof(value), MSG_DONTWAIT), ==, 0);
    close(first);
    close(second);
}

int main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);
    GTestDBus *bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(bus);
    GError *error = NULL;
    test_connection = g_dbus_connection_new_for_address_sync(
            g_test_dbus_get_bus_address(bus),
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
            NULL, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(test_connection);
    g_test_add_func("/application/handles/default", test_default_handles);
    g_test_add_func("/application/handles/fixed", test_fixed_handles);
    g_test_add_func("/application/handles/writeback", test_handle_writeback);
    g_test_add_func("/application/handles/missing", test_missing_objects);
    g_test_add_func("/application/notifications/isolation", test_notification_isolation);
    g_test_add_func("/application/notifications/rejection", test_notification_rejection);
    int result = g_test_run();
    g_dbus_connection_close_sync(test_connection, NULL, &error);
    g_assert_no_error(error);
    g_object_unref(test_connection);
    g_test_dbus_down(bus);
    g_object_unref(bus);
    return result;
}