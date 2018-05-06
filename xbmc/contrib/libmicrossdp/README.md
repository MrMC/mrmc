# libmicrossdp

libmicrossdp is an extremely tiny and portable multicast SSDP discovery client that can be built into a shared library, or statically linked into a final product.

SSDP can be used to find local devices on the network such as:
  - Media consumers
  - Printers / Scanners
  - Internet of Things
  - Telephony devices
  - Network devices
  - Data storage devices

libmicrossdp is **not** a complete UPnP implementation. libmicrossdp does not care about XML schemas or detailed device service inventories. The goal of libmicrossdp is to simply discover UPnP devices on the local network and hand their location data back to the consumer.

## Tested platforms

  - Linux

## Copyright

2014 - 2015 Alexander von Gluck IV <kallisti5@unixzen.com>

## License

To enable wide-spread use, libmicrossdp is released under the MIT license. If you use libmicrossdp in a commercial product, please contribute back upstream!
