# Prototype

This is a PoC test sandbox for prototyping and testing device communication.

## Building
nss, openssl, libpng and libusb are required.
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
