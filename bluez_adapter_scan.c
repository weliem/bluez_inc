/*
 * bluez_adapter_scan.c - Scan for bluetooth devices
 * 	- This example scans for new devices after powering the adapter, if any devices
 * 	  appeared in /org/hciX/dev_XX_YY_ZZ_AA_BB_CC, it is monitered using "InterfaceAdded"
 *	  signal and all the properties of the device is printed
 *	- Scanning continues to run until any device is disappered, this happens after 180 seconds
 *	  automatically if the device is not used.
 * gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -o bluez_adapter_scan ./bluez_adapter_scan.c `pkg-config --libs glib-2.0 gio-2.0`
 */
#include <glib.h>
#include <gio/gio.h>

GDBusConnection *con;

struct InterfaceAdded {
	char *address;
	char *alias;
	char *name;
};


static void interface_added_callback(struct InterfaceAdded *interfaceAdded) {
	g_print("Address : %s\n", interfaceAdded->address);
	g_print("Alias : %s\n", interfaceAdded->alias);
	g_print("Name : %s\n", interfaceAdded->name);
}

static void bluez_property_value(const gchar *key, GVariant *value)
{
	const gchar *type = g_variant_get_type_string(value);

	g_print("\t%s : ", key);
	switch(*type) {
		case 'o':
		case 's':
			g_print("%s\n", g_variant_get_string(value, NULL));
			break;
		case 'b':
			g_print("%d\n", g_variant_get_boolean(value));
			break;
		case 'u':
			g_print("%d\n", g_variant_get_uint32(value));
			break;
		case 'a':
		/* TODO Handling only 'as', but not array of dicts */
			if(g_strcmp0(type, "as"))
				break;
			g_print("\n");
			const gchar *uuid;
			GVariantIter i;
			g_variant_iter_init(&i, value);
			while(g_variant_iter_next(&i, "s", &uuid))
				g_print("\t\t%s\n", uuid);
			break;
		default:
			g_print("Other\n");
			break;
	}
}

static void bluez_device_appeared(GDBusConnection *sig,
				const gchar *sender_name,
				const gchar *object_path,
				const gchar *interface,
				const gchar *signal_name,
				GVariant *parameters,
				gpointer user_data)
{
	(void)sig;
	(void)sender_name;
	(void)object_path;
	(void)interface;
	(void)signal_name;
	(void)user_data;

	GVariantIter *interfaces;
	const char *object;
	const gchar *interface_name;
	GVariant *properties;

	g_variant_get(parameters, "(&oa{sa{sv}})", &object, &interfaces);
	while(g_variant_iter_next(interfaces, "{&s@a{sv}}", &interface_name, &properties)) {
		if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
			g_print("[ %s ]\n", object);
		
			struct InterfaceAdded* x = malloc( sizeof( struct InterfaceAdded));	
			const gchar *property_name;
			GVariantIter i;
			GVariant *prop_val;
			g_variant_iter_init(&i, properties);
			while(g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val)) {
				bluez_property_value(property_name, prop_val);
				if (g_strcmp0(property_name, "Address")==0) {
					const char *addr = g_variant_get_string(prop_val, NULL);
					x->address = (char*) malloc( ( strlen(addr) + 1 ) *sizeof( char ) );
					strcpy(x->address, addr);
				}
			}
			interface_added_callback(x);
			g_variant_unref(prop_val);
		}
		g_variant_unref(properties);
	}
	return;
}

#define BT_ADDRESS_STRING_SIZE 18
static void bluez_device_disappeared(GDBusConnection *sig,
				const gchar *sender_name,
				const gchar *object_path,
				const gchar *interface,
				const gchar *signal_name,
				GVariant *parameters,
				gpointer user_data)
{
	(void)sig;
	(void)sender_name;
	(void)object_path;
	(void)interface;
	(void)signal_name;

	GVariantIter *interfaces;
	const char *object;
	const gchar *interface_name;
	char address[BT_ADDRESS_STRING_SIZE] = {'\0'};

	g_variant_get(parameters, "(&oas)", &object, &interfaces);
	while(g_variant_iter_next(interfaces, "s", &interface_name)) {
		if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
			int i;
			char *tmp = g_strstr_len(object, -1, "dev_") + 4;

			for(i = 0; *tmp != '\0'; i++, tmp++) {
				if(*tmp == '_') {
					address[i] = ':';
					continue;
				}
				address[i] = *tmp;
			}
			g_print("\nDevice %s removed\n", address);
			g_main_loop_quit((GMainLoop *)user_data);
		}
	}
	return;
}

static void bluez_signal_adapter_changed(GDBusConnection *conn,
					const gchar *sender,
					const gchar *path,
					const gchar *interface,
					const gchar *signal,
					GVariant *params,
					void *userdata)
{
	(void)conn;
	(void)sender;
	(void)path;
	(void)interface;
	(void)userdata;

	GVariantIter *properties = NULL;
	GVariantIter *unknown = NULL;
	const char *iface;
	const char *key;
	GVariant *value = NULL;
	const gchar *signature = g_variant_get_type_string(params);

	if(g_strcmp0(signature, "(sa{sv}as)") != 0) {
		g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
		goto done;
	}

	g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
	while(g_variant_iter_next(properties, "{&sv}", &key, &value)) {
		if(!g_strcmp0(key, "Powered")) {
			if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
				g_print("Invalid argument type for %s: %s != %s", key,
						g_variant_get_type_string(value), "b");
				goto done;
			}
			g_print("Adapter is Powered \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
		}
		if(!g_strcmp0(key, "Discovering")) {
			if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
				g_print("Invalid argument type for %s: %s != %s", key,
						g_variant_get_type_string(value), "b");
				goto done;
			}
			g_print("Adapter scan \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
		}
	}
done:
	if(properties != NULL)
		g_variant_iter_free(properties);
	if(value != NULL)
		g_variant_unref(value);
}

static int adapter_call_method(GDBusConnection *connection, const char *adapterPath, const char *method)
{
	GVariant *result;
	GError *error = NULL;

	result = g_dbus_connection_call_sync(connection,
					     "org.bluez",
					     adapterPath,
					     "org.bluez.Adapter1",
					     method,
					     NULL,
					     NULL,
					     G_DBUS_CALL_FLAGS_NONE,
					     -1,
					     NULL,
					     &error);
	if(error != NULL)
		return 1;

	g_variant_unref(result);
	return 0;
}

static int bluez_adapter_set_property(const char *prop, GVariant *value)
{
	GVariant *result;
	GError *error = NULL;

	result = g_dbus_connection_call_sync(con,
					     "org.bluez",
					     "/org/bluez/hci0",
					     "org.freedesktop.DBus.Properties",
					     "Set",
					     g_variant_new("(ssv)", "org.bluez.Adapter1", prop, value),
					     NULL,
					     G_DBUS_CALL_FLAGS_NONE,
					     -1,
					     NULL,
					     &error);
	if(error != NULL)
		return 1;

	g_variant_unref(result);
	return 0;
}

/*
 * Get DBusConnection
 */
 
GDBusConnection* get_dbus_connection() {
	return g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
}

/*  
 * Adapter: StartDiscovery
 */
int adapter_start_discovery(GDBusConnection *connection, const char *adapterPath) {
	return adapter_call_method(connection, adapterPath, "StartDiscovery");
}

/*  
 * Adapter: StopDiscovery
 */
int adapter_stop_discovery(GDBusConnection *connection, const char *adapterPath) {
	return adapter_call_method(connection, adapterPath, "StopDiscovery");
}



int main(void)
{
	GMainLoop *loop;
	int rc;
	guint prop_changed;
	guint iface_added;
	guint iface_removed;
	const char adapter[] = "/org/bluez/hci0";

	con = get_dbus_connection();
	if(con == NULL) {
		g_print("Not able to get connection to system bus\n");
		return 1;
	}

	loop = g_main_loop_new(NULL, FALSE);

	prop_changed = g_dbus_connection_signal_subscribe(con,
						"org.bluez",
						"org.freedesktop.DBus.Properties",
						"PropertiesChanged",
						NULL,
						"org.bluez.Adapter1",
						G_DBUS_SIGNAL_FLAGS_NONE,
						bluez_signal_adapter_changed,
						NULL,
						NULL);

	iface_added = g_dbus_connection_signal_subscribe(con,
							"org.bluez",
							"org.freedesktop.DBus.ObjectManager",
							"InterfacesAdded",
							NULL,
							NULL,
							G_DBUS_SIGNAL_FLAGS_NONE,
							bluez_device_appeared,
							loop,
							NULL);

	iface_removed = g_dbus_connection_signal_subscribe(con,
							"org.bluez",
							"org.freedesktop.DBus.ObjectManager",
							"InterfacesRemoved",
							NULL,
							NULL,
							G_DBUS_SIGNAL_FLAGS_NONE,
							bluez_device_disappeared,
							loop,
							NULL);

	rc = bluez_adapter_set_property("Powered", g_variant_new("b", TRUE));
	if(rc) {
		g_print("Not able to enable the adapter\n");
		goto fail;
	}

	rc = adapter_start_discovery(con, adapter);
	if(rc) {
		g_print("Not able to scan for new devices\n");
		goto fail;
	}

	g_main_loop_run(loop);

	rc = adapter_stop_discovery(con, adapter);
	if(rc)
		g_print("Not able to stop scanning\n");
	g_usleep(100);

/*
	rc = bluez_adapter_set_property("Powered", g_variant_new("b", FALSE));
	if(rc)
		g_print("Not able to disable the adapter\n");
*/
fail:
	g_dbus_connection_signal_unsubscribe(con, prop_changed);
	g_dbus_connection_signal_unsubscribe(con, iface_added);
	g_dbus_connection_signal_unsubscribe(con, iface_removed);
	g_object_unref(con);
	return 0;
}
