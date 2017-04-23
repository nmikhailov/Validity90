# Prototype

This is a PoC test sandbox for prototyping and testing device communication.

## Building
nss, openssl, libpng and libusb are required.
```
make
```

## Running
```
./prototype
```

If you will get permission denied error, do ```sudo chmod a+rwx /dev/bus/usb/<your device path>```
