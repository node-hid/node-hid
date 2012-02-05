# node-hid - Access USB HID devices from node.js #

Prerequisites:

Mac OS (I use 10.6.8) or Linux (I use Ubuntu 11.10 with Linux 3.0 on x86_64)
node.js v0.6, built from source.

Pull the required submodule:

git submodule init
git submodule update

Build the extension:

```
$ cd src/
$ node-waf configure build
```

Try it:

```
$ node show-devices.js
devices: [ { vendorId: 1452,
    productId: 595,
    path: 'USB_05ac_0253_0x100a148e0',
    serialNumber: '',
    manufacturer: 'Apple Inc.',
    product: 'Apple Internal Keyboard / Trackpad',
    release: 280,
    interface: -1 },
  { vendorId: 1452,
    productId: 595,
    path: 'USB_05ac_0253_0x100a14e20',
    serialNumber: '',
    manufacturer: 'Apple Inc.',
    product: 'Apple Internal Keyboard / Trackpad',
    release: 280,
    interface: -1 },
<and more>
```

If you need help, send me an email (hans.huebner@gmail.com)
