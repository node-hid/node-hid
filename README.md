# node-hid - Access USB HID devices from node.js #

## Installation
```
npm install node-hid
```

### Prerequisites:

* [Node.js](https://nodejs.org/) v0.8 - v4.x+
* Mac OS X 10.8, Linux (kernel 2.6+), and Windows XP+
* libudev-dev, libusb-1.0-0-dev (if Linux, see Compile below)
* [git](https://git-scm.com/)

node-hid uses node-pre-gyp to store pre-built binary bundles, so usually no compiler is needed to install.

Platforms we pre-build binaries for:
- Mac OS X x64: v0.10, v0.12, v4.2.x
- Windows x64 & x86: v0.10, v0.12, v4.2.x
- Linux Debian/Ubuntu x64: v4.2.x
- Raspberry Pi arm: v4.2.x

If node-hid doesn't have a pre-built binary for your system, it will attempt to compile locally.  In which case you'll need the **Compiler tools** mentioned below.

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

**NOTE:** Most keyboard & mice-like HID devices cannot be seen by `node-hid` on Windows & Mac.  This is not a limitation of `node-hid` but a limitation of all user-space libraries.  This is a security feature of the OS to prevent input device snooping.

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

## Compiling from source

To compile & develop locally (or if `node-pre-gyp` cannot find a pre-built binary for you), you will need the following tools:
* Mac OS X 10.8+
    * [Xcode](https://itunes.apple.com/us/app/xcode/id497799835?mt=12)
* Windows XP+
    * `npm install --global windows-build-tools`
    * add "%USERPROFILE%\.windows-build-tools\python27" to PATH, like:
        powershell: `$env:Path += ";$env:USERPROFILE\.windows-build-tools\python27"`
    or 
    * [Python 2.7](https://www.python.org/downloads/windows/)
    * [Visual Studio Express 2013 for Desktop](https://www.visualstudio.com/downloads/download-visual-studio-vs#d-2013-express)
    * node-gyp installed globally (`npm install -g node-gyp`)
* Linux (kernel 2.6+)
    * Compiler tools (`apt-get install build-essential git` for Debian/Ubuntu/Raspian)
    * libudev-dev (Fedora: `yum install libusbx-devel`)
    * libusb-1.0-0-dev (Ubuntu versions missing `libusb.h` only)
    * gcc-4.8+ (`apt-get install gcc-4.8 g++-4.8 && export CXX=g++-4.8`)

To build node-hid, check out a copy of this repo, change into its directory, update the submodules, build the node package, then node-pre-gyp to rebuild the C code:

```
git clone https://github.com/node-hid/node-hid.git
cd node-hid                                        # must change into node-hid directory
git submodule update --init                        # done on publish automatically
npm install                                        # rebuilds the module
./node_modules/.bin/node-pre-gyp rebuild           # rebuilds the C code
```

On Windows CMD shell, the last line will instead need to be:
```
.\node_modules\.bin\node-pre-gyp rebuild           # rebuilds the C code
```

You will likely see some warnings from the C compiler as it compiles hidapi.  This is expected.


## Using `node-hid` in Electron projects
In your electron project, add `electron-rebuild` and `electron-prebuilt` to your `devDependencies`.
Then in your package.json `scripts` add:

```
  "postinstall": "electron-rebuild --pre-gyp-fix --force"
```

If you want a specific version of electron, do something like:

```
electron-rebuild -v 0.36.5 --pre-gyp-fix --force -m . -w node-hid
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
