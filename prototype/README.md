# Prototype

This is a PoC test sandbox for prototyping and testing device communication.

## Building
nss, openssl, libpng and libusb are required.
If you are running *UBUNTU* modify the Makefile adding:

```
CFLAGS = -c -Wall -g $(pkg-config --cflags glib-2.0 libusb-1.0)
LDFLAGS = $(pkg-config --libs glib-2.0 libusb-1.0)
```

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
