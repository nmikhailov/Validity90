# Prototype

This is a PoC test sandbox for prototyping and testing device communication.

## Building
Depending on distribution packages like nss(-devel), openssl(-devel), libpng, gnutls(-devel), glib2(-devel) and libusb(-devel) are required.
```
make
make permissions # Will set required permissions
```

## Running
```
./prototype
```
If you get permission denied error, do  
```
sudo chmod a+rwx /dev/bus/usb/<your device path>
sudo chmod a+r /sys/class/dmi/id/product_serial

```
