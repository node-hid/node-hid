# node-hid - Access USB HID devices from node.js #

## Installation
```
npm install node-hid
```

### Prerequisites:

* Mac OS X 10.8, Linux (kernel 2.6+), and Windows XP+
* Node.js v4.x+  (use node-hid v0.4.0 for Node 0.12.7)
* libudev-dev (Linux only)
* libusb-1.0-0-dev (Ubuntu versions missing `libusb.h` only)
* git

### Compile from source for development

To develop locally you'll need the following commands:

```
git submodule update --init   # done automatically on publish, but you'll need it locally
npm install       # rebuilds the module
```

#### Compiling on Windows

Be sure node-gyp is present to compile the extension.
You will need MSVC 2012 or equivalent.

Please note that Windows support is incomplete and needs some work
to pull it to the same level as on Linux and OSX.  See issues #10
and #15 in github for discussion.  Pull requests to improve Windows
support would be welcome.

## Test it

In the `src/` directory, various JavaScript programs can be found
that talk to specific devices in some way.  The `show-devices.js`
program can be used to display all HID devices in the system.

## How to Use

### Load the extension

```
var HID = require('node-hid');
```

### Get a list of all HID devices in the system:

```
var devices = HID.devices()
```

`devices` will contain an array of objects, one for each HID device
available.  Of particular interest are the `vendorId` and
`productId`, as they uniquely identify a device, and the
`path`, which is needed to open a particular device.

Here is some sample output:
```
HID.devices();
[ { vendorId: 1452,
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

### Opening a device

Before a device can be read from or written to, it must be opened:

```
var device = new HID.HID(path);
```

`device` will contain a handle to the device.  The `path` can
be determined by a prior HID.devices() call.  If an error occurs
opening the device, an exception will be thrown.

### Reading from a device

Reading from a device is performed by registering a "data" event
handler:

```
device.on("data", function(data) {});
```

You can also listen for errors like this:

```
device.on("error", function(err) {});
```

All reading is asynchronous.

### Writing to a device

Writing to a device is performed using the write call in a device
handle.  All writing is synchronous.

```
device.write([0x00, 0x01, 0x01, 0x05, 0xff, 0xff]);
```

#### Sending Feature report
To send to a particular HID feature report, include report_id as first byte of array.


## Complete API

```
var device = new HID.HID(path);
```

### Event: "data"

- `chunk` - Buffer - the data read from the device

### Event: "error"

- `error` - The error Object emitted

### device.write(data)

- `data` - the data to be synchronously written to the device

### device.close()

Closes the device. Subsequent reads will raise an error.

### device.pause()

Pauses reading and the emission of `data` events.

### device.resume()

This method will cause the HID device to resume emmitting `data` events.
If no listeners are registered for the `data` event, data will be lost.

When a `data` event is registered for this HID device, this method will
be automatically called.

### device.read(callback)

Low-level function call to initiate an asynchronous read from the device.
`callback` is of the form `callback(err, data)`

### device.sendFeatureReport(data)
- `data` - data of HID feature report, with 0th byte being report_id (`[report_id,...]`)

### device.getFeatureReport(report_id, report_length)
- `report_id` - HID feature report id to get
- `report_length` - length of report

## Support

Please use the [node-hid github issues page](https://github.com/node-hid/node-hid/issues)
for support questions and issues.

