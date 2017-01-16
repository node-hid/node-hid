# node-hid - Access USB HID devices from node.js #

## Installation
```
npm install node-hid
```

### Prerequisites:

* [Node.js](https://nodejs.org/) v0.10, v0.12, v4.2+
* [git](https://git-scm.com/)
* For Linux: (see details at [Compling from source](#compiling-from-source))
    * `libusb-1.0-0 libusb-1.0-0-dev libudev-dev`

#### Supported OSes:
* Linux (kernel 2.6+)
* Mac OS X 10.8+
* Windows XP+

`node-hid` uses `node-pre-gyp` to store pre-built binary bundles, thus no
compiler is needed to install for platforms with pre-built binaries.

Platforms we pre-build binaries for:
- Mac OS X x64: v0.10, v0.12, v4.2.x
- Windows x64 & x86: v0.10, v0.12, v4.2.x

If `node-hid` doesn't have a pre-built binary for your system (e.g. Linux),
`node-gyp` is used to compile `node-hid` locally.  It will need the pre-requisites
listed in [Compling from source](#compiling-from-source) below.

## Test it

In the `src/` directory, various JavaScript programs can be found
that talk to specific devices in some way.  Some interesting ones:
- [`show-devices.js`](https://github.com/node-hid/node-hid/blob/master/src/show-devices.js) - display all HID devices in the system
- [`test-ps3-rumbleled.js`](https://github.com/node-hid/node-hid/blob/master/src/test-ps3-rumbleled.js) - Read PS3 joystick and control its LED & rumblers.
- [`powermate.js`](https://github.com/node-hid/node-hid/blob/master/src/powermate.js) - Read Griffin PowerMate knob and change its LED

To try them out, call them like `node src/showdevices.js` from the node-hid directory.

## How to Use

### Load the module

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

**NOTE:** Most keyboard & mice-like HID devices cannot be seen by `node-hid` on Windows & Mac.
This is not a limitation of `node-hid` but a limitation of all user-space libraries.
This is a security feature of the OS to prevent input device snooping.

### Opening a device

Before a device can be read from or written to, it must be opened.
The `path` can
be determined by a prior HID.devices() call. Use either the `path` from
the list returned by a prior call to `HID.devices()`:
```
var device = new HID.HID(path);
```
or open the first device matching a VID/PID pair:
```
var device = new HID.HID(vid,pid);
```

`device` will contain a handle to the device.
If an error occurs opening the device, an exception will be thrown.

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
Notes:
- The `write()` method sends OUTPUT reports. To send Feature reports,
see the `sendFeatureReport()` method below.
- Some devices use reportIds for OUTPUT reports.  If that is the case,
the first byte of the array to `write()` should be the reportId.


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

### device.readSync()

Return an array of numbers data. If an error occurs, an exception will be thrown.

### device.readTimeout(time_out)

- `time_out` - timeout in milliseconds
Return an array of numbers data. If an error occurs, an exception will be thrown.

### device.sendFeatureReport(data)
- `data` - data of HID feature report, with 0th byte being report_id (`[report_id,...]`)

### device.getFeatureReport(report_id, report_length)
- `report_id` - HID feature report id to get
- `report_length` - length of report

## Notes for Specific Devices

- Xbox 360 Controller on Windows 10 -- does not work

## Linux-specific Notes

### `usage` and `usagePage` device info fields
  These are not available on Linux, only Mac and Windows.
  For reason why, and to ask for its addition, see:
  https://github.com/signal11/hidapi/pull/6

### hidraw support
  To install node-hid with the `hidraw` driver instead of the default libusb one,
  install the "libudev-dev" package and rebuild the library with:
  ```
  npm install node-hid --driver=hidraw
  ```

### udev device permissions
Most Linux distros use `udev` to manage access to physical devices,
and USB HID devices are normally owned by the `root` user.
To allow non-root access, you must create a udev rule for the device,
based on the devices vendorId and productId.

This rule is a file, placed in `/etc/udev/rules.d`, with the lines:
```
SUBSYSTEM=="input", GROUP="input", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="27b8", ATTRS{idProduct}=="01ed", MODE:="666", GROUP="plugdev"
```
(the above is for vendorId = 27b8, productId = 01ed)

Then the udev service is reloaded with: `sudo udevadm control --reload-rules`
For an example, see the
[blink1 udev rules](https://github.com/todbot/blink1/blob/master/linux/51-blink1.rules).

## Compiling from source

To compile & develop locally or if `node-pre-gyp` cannot download a pre-built
binary for you, you will need the following tools:

* All OSes:
    * `node-gyp` installed globally: `npm install -g node-gyp`
    * `node-pre-gyp` installed globally: `npm install -g node-pre-gyp`

* Linux (kernel 2.6+) : install examples shown for Ubuntu
    * Compilation tools: `apt install build-essential git`
    * gcc-4.8+: `apt install gcc-4.8 g++-4.8 && export CXX=g++-4.8`
    * libusb-1.0-0 w/headers:`sudo apt install libusb-1.0-0 libusb-1.0-0-dev`
    * libudev-dev: (Fedora only) `yum install libusbx-devel`

* Mac OS X 10.8+
    * [Xcode](https://itunes.apple.com/us/app/xcode/id497799835?mt=12)

* Windows XP+
    * Visual C++ compiler and Python 2.7, via either:
        * `npm install --global windows-build-tools`
        * add `%USERPROFILE%\.windows-build-tools\python27` to `PATH`, like:
            PowerShell: `$env:Path += ";$env:USERPROFILE\.windows-build-tools\python27"`
        or:
        * [Python 2.7](https://www.python.org/downloads/windows/)
        * [Visual Studio Express 2013 for Desktop](https://www.visualstudio.com/downloads/download-visual-studio-vs#d-2013-express)

To build `node-hid` for development:
* check out a copy of this repo
* change into its directory
* update the submodules
* build the node package
* node-pre-gyp to rebuild the C code

For example:

```
git clone https://github.com/node-hid/node-hid.git
cd node-hid                                        # must change into node-hid directory
git submodule update --init                        # done on publish automatically
npm install                                        # rebuilds the module
node-pre-gyp rebuild                               # rebuilds the C code
node src/show-devices.js
```

You will likely see some warnings from the C compiler as it compiles
[hidapi](https://github.com/signal11/hidapi) (the underlying C library `node-hid` uses).  
This is expected.


## Using `node-hid` in Electron projects
In your electron project, add `electron-rebuild` and `electron-prebuilt` to your `devDependencies`.
Then in your package.json `scripts` add:

```
  "postinstall": "electron-rebuild --force"
```

If you want a specific version of electron, do something like:

```
electron-rebuild -v 0.36.5 --force -m . -w node-hid
```

## Using `node-hid` in NW.js projects

```
    npm install node-pre-gyp
    ./node_modules/.bin/node-pre-gyp rebuild --runtime=node-webkit --target=0.12.3
```   

You can change 0.12.3 to version nwjs that you want to deploy.

## Support

Please use the [node-hid github issues page](https://github.com/node-hid/node-hid/issues)
for support questions and issues.
