# Bluez in C


The goal of this library is to provide a clean C interface to Bluez, without needing to use DBus commands. Using Bluez over the DBus is quite tricky and this library does all the hard work under the hood. 
As a result, it looks like a 'normal' C library for Bluetooth.

Todo
* Make StartDiscovery and StopDiscovery async
* Make power on/off async
* Allow for fixed pin code to be registered for pairing
* Provide callbacks for pincode and passcodes
* Make agent path configurable
* Implement Pair and CancelPairing on Device
* Add more device property functions
* Check for memory leaks with valgrind
* Use 'src' and 'include' folders in project
* Make it a library

## Discovering devices

In order to discover devices, you first need to get hold of a Bluetooth *adapter*. 
You do this by calling `binc_get_default_adapter()` with your DBus connection as an argument:

```c
GDBusConnection *dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
Adapter *adapter = binc_get_default_adapter(dbusConnection);
```

The next step is to set any scanfilters and set the callback you want to receive the found devices on:

```c
binc_adapter_set_discovery_callback(default_adapter, &on_scan_result);
binc_adapter_set_discovery_filter(default_adapter, -100);
binc_adapter_start_discovery(default_adapter);
```

The discovery will deliver all found devices on the callback you provided. You typically check if it is the device you are looking for, stop the discovery and then connect to it:

```c
void on_scan_result(Adapter *adapter, Device *device) {
    const char* name = binc_device_get_name(device);
    if (name != NULL && g_str_has_prefix(name, "Polar")) {
        binc_adapter_stop_discovery(adapter);
        binc_device_register_connection_state_change_callback(device, &on_connection_state_changed);
        binc_device_register_services_resolved_callback(device, &on_services_resolved);
        binc_device_set_read_char_callback(device, &on_read);
        binc_device_set_write_char_callback(device, &on_write);
        binc_device_set_notify_char_callback(device, &on_notify);
        binc_device_connect(device);
    }
}
```
As you can see, just before connecting, you must set up some callbacks for receiving connection state changes and the results of reading/writing to characteristics.

## Reading an writing characteristics

...
