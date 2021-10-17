# Bluez in C


The goal of this library is to provide a clean C interface to Bluez, without needing to use DBus commands. Using Bluez over the DBus is quite tricky to say the least, and this library does all the hard work under the hood. 
As a result, it looks like a 'normal' C library for Bluetooth!


## Discovering devices

In order to discover devices, you first need to get hold of a Bluetooth *adapter*. 
You do this by calling `binc_get_default_adapter()` with your DBus connection as an argument:

```c
int main(void) {
    GDBusConnection *dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    Adapter *adapter = binc_get_default_adapter(dbusConnection);
    
    //...
}
```

The next step is to set any scanfilters and set the callback you want to receive the found devices on:

```c
int main(void) {
    
    // ...
    
    binc_adapter_set_discovery_callback(default_adapter, &on_scan_result);
    binc_adapter_set_discovery_filter(default_adapter, -100, NULL);
    binc_adapter_start_discovery(default_adapter);
}
```

The discovery will deliver all found devices on the callback you provided. You typically check if it is the device you are looking for, stop the discovery and then connect to it:

```c
void on_scan_result(Adapter *adapter, Device *device) {
    const char* name = binc_device_get_name(device);
    if (name != NULL && g_str_has_prefix(name, "Polar")) {
        binc_adapter_stop_discovery(adapter);
        binc_device_set_connection_state_change_callback(device, &on_connection_state_changed);
        binc_device_set_services_resolved_callback(device, &on_services_resolved);
        binc_device_set_read_char_callback(device, &on_read);
        binc_device_set_write_char_callback(device, &on_write);
        binc_device_set_notify_char_callback(device, &on_notify);
        binc_device_connect(device);
    }
}
```
As you can see, just before connecting, you must set up some callbacks for receiving connection state changes and the results of reading/writing to characteristics.

## Connecting, service discovery and disconnecting

You connect by calling `binc_device_connect(device)`. Then the following sequence will happen:
* when the device is connected the *connection_state* changes and your registered callback will be called. However, you cannot use the device yet because the service discovery has not yet been done.
* Bluez will then start the service discovery automatically and when it finishes, the *services_resolved* callback is called. So the *service_resolved* callback is the right place to start reading and writing characteristics or starting notifications. 

```c
void on_connection_state_changed(Device *device, ConnectionState state, GError *error) {
    if (error != NULL) {
        log_debug(TAG, "(dis)connect failed (error %d: %s)", error->code, error->message);
        return;
    }

    log_debug(TAG, "'%s' (%s) state: %s (%d)", binc_device_get_name(device), binc_device_get_address(device), binc_device_get_connection_state_name(device), state);
    if (state == DISCONNECTED && binc_device_get_bonding_state(device) != BONDED) {
        binc_adapter_remove_device(default_adapter, device);
    }
}
```

If a connection attempt fails or times out after 25 seconds, the *connection_state* callback is called with an error.

To disconnect a connected device, call `binc_device_disconnect(device)` and the device will be disconnected. Again, the *connection_state* callback will be called. If you want to remove the device from the DBus after disconnecting, you call `binc_adapter_remove_device(adapter, device)`. 

## Reading and writing characteristics

We can start using characteristics once the service discovery has been completed. 
Typically, we read some characteristics like its model and manufacturer. In order to read or write, you first need to get the **Characteristic** by using `binc_device_get_characteristic(device, DIS_SERVICE, MANUFACTURER_CHAR)`. 
You need to provide the **service UUID** and **characteristic UUID** to find a characteristic:

```c
void on_services_resolved(Device *device) {
    Characteristic *manufacturer = binc_device_get_characteristic(device, DIS_SERVICE, MANUFACTURER_CHAR);
    if (manufacturer != NULL) {
        binc_characteristic_read(manufacturer);
    }

    Characteristic *model = binc_device_get_characteristic(device, DIS_SERVICE, MODEL_CHAR);
    if (model != NULL) {
        binc_characteristic_read(model);
    }
}
```

Like all BLE operations, reading and writing are **asynchronous** operations. So you issue them and they will complete immediately, but you then have to wait for the result to come in on a callback. You register your callback by calling `binc_device_set_read_char_callback(device, &on_read)`. 

```c
void on_read(Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    const char* uuid = binc_characteristic_get_uuid(characteristic);
    if (byteArray != NULL) {
        if (g_str_equal(uuid, MANUFACTURER_CHAR)) {
            log_debug(TAG, "manufacturer = %s", byteArray->data);
        } else if (g_str_equal(uuid, MODEL_CHAR)) {
            log_debug(TAG, "model = %s", byteArray->data);
        }
    }

    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
    }
}
```

Writing to characteristics works in a similar way. Register your callback using `binc_device_set_write_char_callback(device, &on_write)`. Make sure you check if you can write to the characteristic before attempting it:

```c
void on_services_resolved(Device *device) {

    // ...
    
    Characteristic *current_time = binc_device_get_characteristic(device, CTS_SERVICE, CURRENT_TIME_CHAR);
    if (current_time != NULL) {
        if (binc_characteristic_supports_write(current_time,WITH_RESPONSE)) {
            GByteArray *timeBytes = binc_get_current_time();
            binc_characteristic_write(current_time, timeBytes, WITH_RESPONSE);
            g_byte_array_free(timeBytes, TRUE);
        }
    }
}
```

## Receiving notifications

Bluez treats notifications and indications in the same way, calling them 'notifications'. If you want to receive notifications you have to 'start' them by calling `binc_characteristic_start_notify()`. As usual, first register your callback by calling `binc_device_set_notify_char_callback(device, &on_notify)`. Here is an example:

```c
void on_services_resolved(Device *device) {

    // ...
    
    Characteristic *temperature = binc_device_get_characteristic(device, HTS_SERVICE, TEMPERATURE_CHAR);
    if (temperature != NULL) {
        binc_characteristic_start_notify(temperature);
    }
}    
```

And then receiving your notifications:

```c
void on_notify(Characteristic *characteristic, GByteArray *byteArray) {
    const char* uuid = binc_characteristic_get_uuid(characteristic);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    parser_set_offset(parser, 1);
    if (g_str_equal(uuid, TEMPERATURE_CHAR)) {
        float value = parser_get_float(parser);
        log_debug(TAG, "temperature %.1f", value);
    } 
    parser_free(parser);
}
```

## Bluez documentation

The official Bluez documentation is a bit sparse but can be found here: 
* [Adapter documentation](https://github.com/bluez/bluez/blob/master/doc/adapter-api.txt) (for adapter)
* [GATT documentation](https://github.com/bluez/bluez/blob/master/doc/gatt-api.txt) (for service, characteristics and descriptors)
* [Device documentation](https://github.com/bluez/bluez/blob/master/doc/device-api.txt) (for device)
* [Agent documentation](https://github.com/bluez/bluez/blob/master/doc/agent-api.txt) (for agent)

You will notice that most original methods and properties are available in this library. In some cases, some adaptations have been done for convenience.
