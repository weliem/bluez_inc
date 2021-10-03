# Bluez in C


The goal of this library is to provide a clean C interface to Bluez, without needing to use DBus. Using Bluez directly using the DBus is quite tricky and this library does all the hard work under the hood.

Todo
* Allow for fixed pin code to be registered for pairing
* Provide callbacks for pincode and passcodes
* Make agent path configurable
* Implement Pair and CancelPairing on Device
* Add more device property functions
* Check for memory leaks with valgrind
* Use 'src' and 'include' folders in project
* Make it a library