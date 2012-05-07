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

# Windows Setup #
This source works on Win7 as of (5/7/12). Here are the steps to build on Windows.
Use node-gyp... the instruction in the README worked for me https://github.com/TooTallNate/node-gyp/blob/master/README.md

After you have node-gyp installed on your windows node.js install.

```
...> cd _path\to\nodejs\modules\node-hid_
...> node-gyp configure
...> node-gyp build
```
you should now have a HID.node file in the node-hid\build\(Release or Debug) folder.
Copy HID.node to your nodejs\node_modules folder and run node..
```
> var HID = require('HID.node');
undefined
> HID.devices()
[ { vendorId: 1133,
    productId: 50479,
    path: '\\\\?\\hid#vid_046d&pid_c52f&mi_00#8&7764ed8&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}',
    serialNumber: '\t',
    manufacturer: 'Logitech',
    product: 'USB Receiver',
    release: 5632,
    interface: 0 },
  { vendorId: 1226,
    productId: 34,
    path: '\\\\?\\hid#vid_04ca&pid_0022#7&3974be35&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}',
    serialNumber: '\t',
    manufacturer: 'LITEON Technology',
    product: 'USB Keyboard',
    release: 265,
    interface: -1 } ]
>
```
You should be good to go...
