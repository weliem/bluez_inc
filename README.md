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

## Scanning

In order to scan, you first need to get hold of a Bluetooth *adapter*. 
You do this by calling 'binc_get_default_adapter()':

'''c
GDBusConnection *dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
Adapter *adapter = binc_get_default_adapter(dbusConnection);
'''