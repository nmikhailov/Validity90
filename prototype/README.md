# Prototype

This is a PoC test sandbox for prototyping and testing device communication.

## Prepare build environment

Depending on distribution packages like nss(-devel), openssl(-devel), libpng, gnutls(-devel), glib2(-devel), libusb-1.0(-devel), libnss3-dev might be needed.

### Dependencies for Ubuntu

```
sudo apt-get install make gcc libgcrypt-dev libglib2.0-dev libnss3-dev libusb-1.0-0-dev libssl-dev libpng-dev libgnutls-dev
```


## Build

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
