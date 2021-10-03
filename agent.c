//
// Created by martijn on 25/9/21.
//

#include "agent.h"
#include "adapter.h"
#include "logger.h"
#include <stdint-gcc.h>

#define TAG "Agent"
#define AGENT_PATH "/org/bluez/BincAgent"

struct binc_agent {
    char *path;
    IoCapability io_capability;
    GDBusConnection *connection;
    Adapter *adapter;
    guint registration_id;
};

static void bluez_agent_method_call(GDBusConnection *conn,
                                    const gchar *sender,
                                    const gchar *path,
                                    const gchar *interface,
                                    const gchar *method,
                                    GVariant *params,
                                    GDBusMethodInvocation *invocation,
                                    void *userdata) {
    int pass;
    int entered;
    char *object_path = NULL;
    GVariant *p = g_dbus_method_invocation_get_parameters(invocation);

    //   log_debug(TAG, "agent called: %s()", method);

    Adapter *adapter = (Adapter *) userdata;
    g_assert(adapter != NULL);

    if (!strcmp(method, "RequestPinCode")) { ;
    } else if (!strcmp(method, "DisplayPinCode")) { ;
    } else if (!strcmp(method, "RequestPasskey")) {

        log_debug(TAG, "Getting the Pin from user: ");
        //       fscanf(stdin, "%d", &pass);
        g_print("\n");
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", pass));
    } else if (!strcmp(method, "DisplayPasskey")) {
        g_variant_get(params, "(ouq)", &object_path, &pass, &entered);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (!strcmp(method, "RequestConfirmation")) {
        g_variant_get(params, "(ou)", &object_path, &pass);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (!strcmp(method, "RequestAuthorization")) {
        g_variant_get(params, "(o)", &object_path);
        log_debug(TAG, "request for authorization %s", object_path);
        Device *device = binc_adapter_get_device_by_path(adapter, object_path);
        if (device != NULL) {
            binc_device_set_bonding_state(device, BONDING);
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (!strcmp(method, "AuthorizeService")) {
        log_debug(TAG, "authorize service");
    } else if (!strcmp(method, "Cancel")) {
        log_debug(TAG, "cancelling pairing");
    } else
        g_print("We should not come here, unknown method\n");
}

static const GDBusInterfaceVTable agent_method_table = {
        .method_call = bluez_agent_method_call,
};

static int bluez_register_agent(Agent *agent) {
    GError *error = NULL;
    guint id = 0;
    GDBusNodeInfo *info = NULL;

    static const gchar bluez_agent_introspection_xml[] =
            "<node name='/org/bluez/SampleAgent'>"
            "   <interface name='org.bluez.Agent1'>"
            "       <method name='Release'>"
            "       </method>"
            "       <method name='RequestPinCode'>"
            "           <arg type='o' name='device' direction='in' />"
            "           <arg type='s' name='pincode' direction='out' />"
            "       </method>"
            "       <method name='DisplayPinCode'>"
            "           <arg type='o' name='device' direction='in' />"
            "           <arg type='s' name='pincode' direction='in' />"
            "       </method>"
            "       <method name='RequestPasskey'>"
            "           <arg type='o' name='device' direction='in' />"
            "           <arg type='u' name='passkey' direction='out' />"
            "       </method>"
            "       <method name='DisplayPasskey'>"
            "           <arg type='o' name='device' direction='in' />"
            "           <arg type='u' name='passkey' direction='in' />"
            "           <arg type='q' name='entered' direction='in' />"
            "       </method>"
            "       <method name='RequestConfirmation'>"
            "           <arg type='o' name='device' direction='in' />"
            "           <arg type='u' name='passkey' direction='in' />"
            "       </method>"
            "       <method name='RequestAuthorization'>"
            "           <arg type='o' name='device' direction='in' />"
            "       </method>"
            "       <method name='AuthorizeService'>"
            "           <arg type='o' name='device' direction='in' />"
            "           <arg type='s' name='uuid' direction='in' />"
            "       </method>"
            "       <method name='Cancel'>"
            "       </method>"
            "   </interface>"
            "</node>";

    info = g_dbus_node_info_new_for_xml(bluez_agent_introspection_xml, &error);
    if (error) {
        g_printerr("Unable to create node: %s\n", error->message);
        g_clear_error(&error);
        return 0;
    }

    agent->registration_id = g_dbus_connection_register_object(agent->connection,
                                                               AGENT_PATH,
                                                               info->interfaces[0],
                                                               &agent_method_table,
                                                               agent->adapter, NULL, &error);
    g_dbus_node_info_unref(info);
    //g_dbus_connection_unregister_object(con, id);
    /* call register method in AgentManager1 interface */
    return id;
}

static int binc_agentmanager_call_method(GDBusConnection *connection, const gchar *method, GVariant *param) {
    g_assert(connection != NULL);
    g_assert(method != NULL);

    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(connection,
                                         "org.bluez",
                                         "/org/bluez",
                                         "org.bluez.AgentManager1",
                                         method,
                                         param,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if (error != NULL) {
        g_print("Register %s: %s\n", AGENT_PATH, error->message);
        return 1;
    }

    g_variant_unref(result);
    return 0;
}

int binc_agentmanager_register_agent(Agent *agent) {
    g_assert(agent != NULL);
    char *capability = NULL;

    switch (agent->io_capability) {
        case DISPLAY_ONLY:
            capability = "DisplayOnly";
            break;
        case DISPLAY_YES_NO:
            capability = "DisplayYesNo";
            break;
        case KEYBOARD_ONLY:
            capability = "KeyboardOnly";
            break;
        case NO_INPUT_NO_OUTPUT:
            capability = "NoInputNoOutput";
            break;
        case KEYBOARD_DISPLAY:
            capability = "KeyboardDisplay";
            break;
    }
    int result = binc_agentmanager_call_method(agent->connection, "RegisterAgent",
                                               g_variant_new("(os)", agent->path, capability));
    if (result == EXIT_FAILURE) {
        log_debug(TAG, "failed to register agent");
    }

    result = binc_agentmanager_call_method(agent->connection, "RequestDefaultAgent", g_variant_new("(o)", agent->path));
    if (result == EXIT_FAILURE) {
        log_debug(TAG, "failed to register agent as default agent");
    }
}

void binc_agent_free(Agent *agent) {
    g_assert (agent != NULL);
    gboolean result = g_dbus_connection_unregister_object(agent->connection, agent->registration_id);
    if (!result) {
        log_debug(TAG, "could not unregister agent");
    }
    g_free(agent);
}

Agent *binc_agent_create(Adapter *adapter, IoCapability io_capability) {
    Agent *agent = g_new0(Agent, 1);
    agent->path = AGENT_PATH;
    agent->connection = binc_adapter_get_dbus_connection(adapter);
    agent->adapter = adapter;
    agent->io_capability = io_capability;
    bluez_register_agent(agent);
    binc_agentmanager_register_agent(agent);
    return agent;
}

