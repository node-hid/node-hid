# node-hid - Access USB HID devices from Node.js #

[![npm](https://img.shields.io/npm/dm/node-hid.svg?maxAge=2592000)](http://npmjs.com/package/node-hid)
[![Prebuild](https://github.com/node-hid/node-hid/actions/workflows/build.yml/badge.svg)](https://github.com/node-hid/node-hid/actions/workflows/build.yml)


* [node-hid - Access USB HID devices from Node.js](#node-hid---access-usb-hid-devices-from-nodejs)
  * [Platform Support](#platform-support)
     * [Supported Platforms](#supported-platforms)
     * [Supported Node versions](#supported-node-versions)
     * [Supported Electron versions](#supported-electron-versions)
  * [Installation](#installation)
     * [Installation Special Cases](#installation-special-cases)
  * [Examples](#examples)
  * [Async API Usage](#async-api-usage)
  * [Sync API Usage](#sync-api-usage)
  * [Complete Async API](#complete-async-api)
  * [Complete Sync API](#complete-sync-api)
  * [General notes:](#general-notes)
     * [Thread safety, Worker threads, Context-aware modules](#thread-safety-worker-threads-context-aware-modules)
     * [Devices node-hid cannot read](#devices-node-hid-cannot-read)
  * [Mac notes](#mac-notes)
  * [Windows notes](#windows-notes)
     * [Xbox 360 Controller on Windows 10](#xbox-360-controller-on-windows-10)
  * [Linux notes](#linux-notes)
     * [udev device permissions](#udev-device-permissions)
     * [Selecting driver type](#selecting-driver-type)
  * [Compiling from source](#compiling-from-source)
     * [Linux (kernel 2.6 ) : (install examples shown for Debian/Ubuntu)](#linux-kernel-26--install-examples-shown-for-debianubuntu)
     * [FreeBSD](#freebsd)
     * [Mac OS X 10.8 ](#mac-os-x-108)
     * [Windows 7, 8, 10](#windows-7-8-10)
     * [Building node-hid from source, for your projects](#building-node-hid-from-source-for-your-projects)
     * [Build node-hid for <code>node-hid</code> development](#build-node-hid-for-node-hid-development)
     * [Building node-hid for cross-compiling](#building-node-hid-for-cross-compiling)
  * [Electron projects using node-hid](#electron-projects-using-node-hid)
  * [NW.js projects using node-hid](#nwjs-projects-using-node-hid)
  * [Support](#support)

## Platform Support
`node-hid` currently supports Node.js v10 and upwards. For versions before that, you will need to use an older version.
The platforms, architectures and node versions `node-hid` supports are the following.
In general we try to provide pre-built native library binaries for the most common platforms, Node and Electron versions.

We strive to make `node-hid` cross-platform so there's a good chance any
combination not listed here will compile and work.

### Supported Platforms ###
- Windows x86 (32-bit)
- Windows x64 (64-bit)
- Mac OSX 10.9+
- Linux x64 (²)
- Linux x86 (¹)
- Linux ARM / Raspberry Pi / Various SBC (²)
- Linux ARM64 / Various SBC (²)
- Linux MIPSel (¹)
- Linux PPC64 (¹)

¹ prebuilt-binaries not provided for these platforms  
² prebuilt binary built on Debian 10 Buster

### Supported Node versions ###

* Node v10 to
* Node v20

### Supported Electron versions ###

* Electron v3 to
* Electron v24

Future versions of Node or Electron should work with no extra work, since `node-hid` is now based on NAPI.

## Installation

For most "standard" use cases (macOS, Windows, Linux x86), `node-hid` will install like a standard npm package:

```
npm install node-hid
```

If you install globally, the test program `src/show-devices.js` is installed as `hid-showdevices`. On Linux you can use it to try the difference between `hidraw` and `libusb` driverTypes:
```
$ npm install -g node-hid
$ hid-showdevices libusb
$ hid-showdevices hidraw
```

### Installation Special Cases

We are using [prebuild](https://github.com/mafintosh/prebuild) to compile and post binaries of the library for most common use cases (Linux, MacOS, Windows on standard processor platforms). If a prebuild is not available, `node-hid` will work, but `npm install node-hid` will compile the binary when you install.  For more details on compiler setup, see  [Compling from source](#compiling-from-source) below.

## Examples

In the `src/` directory, various JavaScript programs can be found
that talk to specific devices in some way.  Some interesting ones:
- [`show-devices.js`](./src/show-devices.js) - display all HID devices in the system
- [`test-ps3-rumbleled.js`](./src/test-ps3-rumbleled.js) - Read PS3 joystick and control its LED & rumblers
- [`test-powermate.js`](./src/test-powermate.js) - Read Griffin PowerMate knob and change its LED
- [`test-blink1.js`](./src/test-blink1.js) - Fade colors on blink(1) RGB LED
- [`test-bigredbutton.js`](./src/test-bigredbutton.js) - Read Dreamcheeky Big Red Button
- [`test-teensyrawhid.js`](./src/test-teensyrawhid.js) - Read/write Teensy running RawHID "Basic" Arduino sketch

To try them out, run them with `node src/showdevices.js` from within the node-hid directory.

----
## Async vs sync API

Since 3.0.0, `node-hid` supports both the old synchronous api, and a newer async api.
It is recommended to use the async api to avoid `node-hid` from blocking your code from executing. For prototyping or tiny applications, this likely will not matter, but for npm libraries or larger applications it can be problematic.

Additionally, the sync api is limited to only beind able to read up to the `UV_THREADPOOL_SIZE` (default is 4) number of devices at once. Reading from multiple could degrade performance of your application, as there will be fewer than expected uv workers available for nodejs and other libraries to use for other tasks.

The async API is identical to the sync API described below, except every method returns a `Promise` that must be handled. Any unhandled promise can crash your application.

It is safe to use the sync api for some devices in an application, and the async api for other devices. The thread safety of `hidapi` is handled for you here to avoid crashes.

## Cost of `HID.devices()`, `HID.devicesAsync()`, `new HID.HID()` and `HIDAsync.open()` for detecting device plug/unplug
All of `HID.devices()`, `HID.devicesAsync()`, `new HID.HID()` and `HIDAsync.open()` are relatively costly, each causing a USB (and potentially Bluetooth) enumeration. This takes time and OS resources. Doing either can slow down the read/write that you do in parallel with a device, and cause other USB devices to slow down too. This is how USB works.

If you are polling `HID.devices()` or `HID.devicesAsync()` or other inefficient methods to detect device plug / unplug, consider instead using [node-usb](https://github.com/node-usb/node-usb#usbdetection). `node-usb` uses OS-specific, non-bus enumeration ways to detect device plug / unplug.


## Async API Usage

### List all HID devices connected

```js
var HID = require('node-hid');
var devices = await HID.devicesAsync();
```

`devices` will contain an array of objects, one for each HID device
available.  Of particular interest are the `vendorId` and
`productId`, as they uniquely identify a device, and the
`path`, which is needed to open a particular device.

Sample output:

```js
await HID.devicesAsync();
{ vendorId: 10168,
    productId: 493,
    path: 'IOService:/AppleACPIPl...HIDDevice@14210000,0',
    serialNumber: '20002E8C',
    manufacturer: 'ThingM',
    product: 'blink(1) mk2',
    release: 2,
    interface: -1,
    usagePage: 65280,
    usage: 1 },
  { vendorId: 1452,
    productId: 610,
    path: 'IOService:/AppleACPIPl...Keyboard@14400000,0',
    serialNumber: '',
    manufacturer: 'Apple Inc.',
    product: 'Apple Internal Keyboard / Trackpad',
    release: 549,
    interface: -1,
    usagePage: 1,
    usage: 6 },
    <and more>
```


### Opening a device

Before a device can be read from or written to, it must be opened.
Use either the `path` from the list returned by a prior call to `HID.devicesAsync()`:

```js
var device = await HID.HIDAsync.open(path);
```

or open the first device matching a VID/PID pair:

```js
var device = await HID.HIDAsync.open(vid,pid);
```

If set to true the `nonExclusive` property allows the device to be opened in non-exclusive mode.
If the flag is not set the behaviour will default to that of the specific os.

```js
// With path
var device = await HID.HIDAsync.open(path, { nonExclusive: true });

// With vid, pid
var device = await HID.HIDAsync.open(vid,pid, { nonExclusive: true });
```

The `device` variable will contain a handle to the device.
If an error occurs opening the device, an exception will be thrown.

A `node-hid` device is an `EventEmitter`.
While it shares some method names and usage patterns with
`Readable` and `Writable` streams, it is not a stream and the semantics vary.
For example, `device.write` does not take encoding or callback args and
`device.pause` does not do the same thing as `readable.pause`.
There is also no `pipe` method.

### Reading from a device

To receive FEATURE reports, use `await device.getFeatureReport()`.

To receive INPUT reports, use `device.on("data",...)`.
A `node-hid` device is an EventEmitter.
Reading from a device is performed by registering a "data" event handler:

```js
device.on("data", function(data) {});
```

You can also listen for errors like this:

```js
device.on("error", function(err) {});
```
For FEATURE reports:

```js
var buf = await device.getFeatureReport(reportId, reportLength)
```


Notes:
- Reads via `device.on("data")` are asynchronous
- To remove an event handler, close the device with `device.close()`
- When there is not yet a data handler or no data handler exists,
   data is not read at all -- there is no buffer.

### Writing to a device

To send FEATURE reports, use `device.sendFeatureReport()`.  

To send OUTPUT reports, use `device.write()`.

The ReportId is the first byte of the array sent to `device.sendFeatureReport()` or `device.write()`, meaning the array should be one byte bigger than your report.
If your device does NOT use numbered reports, set the first byte of the 0x00.


```js
device.write([0x00, 0x01, 0x01, 0x05, 0xff, 0xff]);
```
```js
device.sendFeatureReport( [0x01, 'c', 0, 0xff,0x33,0x00, 70,0, 0] );
```
Notes:
- All writes and other operations performed with the HIDAsync device are done in a work-queue, so will happen in the order you issue them with the returned `Promise` resolving once the operation is completed
- You must send the exact number of bytes for your chosen OUTPUT or FEATURE report.
- Both `device.write()` and `device.sendFeatureReport()` return a Promise containing the number of bytes written + 1.
- For devices using Report Ids, the first byte of the array to `write()` or `sendFeatureReport()` must be the Report Id.


## Sync API Usage

### List all HID devices connected

```js
var HID = require('node-hid');
var devices = HID.devices();
```

`devices` will contain an array of objects, one for each HID device
available.  Of particular interest are the `vendorId` and
`productId`, as they uniquely identify a device, and the
`path`, which is needed to open a particular device.

Sample output:

```js
HID.devices();
{ vendorId: 10168,
    productId: 493,
    path: 'IOService:/AppleACPIPl...HIDDevice@14210000,0',
    serialNumber: '20002E8C',
    manufacturer: 'ThingM',
    product: 'blink(1) mk2',
    release: 2,
    interface: -1,
    usagePage: 65280,
    usage: 1 },
  { vendorId: 1452,
    productId: 610,
    path: 'IOService:/AppleACPIPl...Keyboard@14400000,0',
    serialNumber: '',
    manufacturer: 'Apple Inc.',
    product: 'Apple Internal Keyboard / Trackpad',
    release: 549,
    interface: -1,
    usagePage: 1,
    usage: 6 },
    <and more>
```


### Opening a device

Before a device can be read from or written to, it must be opened.
Use either the `path` from the list returned by a prior call to `HID.devices()`:

```js
var device = new HID.HID(path);
```

or open the first device matching a VID/PID pair:

```js
var device = new HID.HID(vid,pid);
```

If set to true the `nonExclusive` property allows the device to be opened in non-exclusive mode
If the flag is not set the behaviour will default to that of the specific os.

```js
// With path
var device = await HID.HID(path, { nonExclusive: true });

// With vid, pid
var device = await HID.HID(vid,pid, { nonExclusive: true });
```

The `device` variable will contain a handle to the device.
If an error occurs opening the device, an exception will be thrown.

A `node-hid` device is an `EventEmitter`.
While it shares some method names and usage patterns with
`Readable` and `Writable` streams, it is not a stream and the semantics vary.
For example, `device.write` does not take encoding or callback args and
`device.pause` does not do the same thing as `readable.pause`.
There is also no `pipe` method.

### Picking a device from the device list
If you need to filter down the `HID.devices()` list, you can use
standard Javascript array techniques:
```js
var devices = HID.devices();
var deviceInfo = devices.find( function(d) {
    var isTeensy = d.vendorId===0x16C0 && d.productId===0x0486;
    return isTeensy && d.usagePage===0xFFAB && d.usage===0x200;
});
if( deviceInfo ) {
  var device = new HID.HID( deviceInfo.path );
  // ... use device
}
```
You can also find device of interest by passing VID and PID   
```js
//return all the devices that match specified VID and PID
var devices = HID.devices(0x16C0,0x0486);
```

### Reading from a device

To receive FEATURE reports, use `device.getFeatureReport()`.

To receive INPUT reports, use `device.on("data",...)`.
A `node-hid` device is an EventEmitter.
Reading from a device is performed by registering a "data" event handler:

```js
device.on("data", function(data) {});
```

You can also listen for errors like this:

```js
device.on("error", function(err) {});
```
For FEATURE reports:

```js
var buf = device.getFeatureReport(reportId, reportLength)
```


Notes:
- Reads via `device.on("data")` are asynchronous
- Reads via `device.getFeatureReport()` are synchronous
- To remove an event handler, close the device with `device.close()`
- When there is not yet a data handler or no data handler exists,
   data is not read at all -- there is no buffer.

### Writing to a device

To send FEATURE reports, use `device.sendFeatureReport()`.  

To send OUTPUT reports, use `device.write()`.

All writing is synchronous.

The ReportId is the first byte of the array sent to `device.sendFeatureReport()` or `device.write()`, meaning the array should be one byte bigger than your report.
If your device does NOT use numbered reports, set the first byte of the 0x00.


```js
device.write([0x00, 0x01, 0x01, 0x05, 0xff, 0xff]);
```
```js
device.sendFeatureReport( [0x01, 'c', 0, 0xff,0x33,0x00, 70,0, 0] );
```
Notes:
- You must send the exact number of bytes for your chosen OUTPUT or FEATURE report.
- Both `device.write()` and `device.sendFeatureReport()` return
number of bytes written + 1.
- For devices using Report Ids, the first byte of the array to `write()` or
`sendFeatureReport()` must be the Report Id.


## Complete Async API

### `devices = await HID.devicesAsync()`

- Return array listing all connected HID devices

### `devices = await HID.devicesAsync(vid,pid)`

- Return array listing all connected HID devices with specific VendorId and ProductId

### `device = await HID.HIDAsync.open(path,options?:{nonExclusive?:boolean})`

- Open a HID device at the specified platform-specific path

### `device = await HID.HIDAsync.open(vid,pid,options?:{nonExclusive?:boolean})`

- Open first HID device with specific VendorId and ProductId

### `device.on('data', function(data) {} )`

- `data` - Buffer - the data read from the device

### `device.on('error, function(error) {} )`

- `error` - The error Object emitted

### `device.write(data)`

- `data` - the data to be synchronously written to the device,
first byte is Report Id or 0x00 if not using numbered reports.
- Returns number of bytes actually written

### `device.close()`

- Closes the device. Subsequent reads will raise an error.

### `device.pause()`

- Pauses reading and the emission of `data` events.  
This means the underlying device is _silenced_ until resumption --
it is not like pausing a stream, where data continues to accumulate.

### `device.resume()`

- This method will cause the HID device to resume emmitting `data` events.
If no listeners are registered for the `data` event, data will be lost.

- When a `data` event is registered for this HID device, this method will
be automatically called.

### `device.read(time_out)`

- (optional) `time_out` - timeout in milliseconds
- Low-level function call to initiate an asynchronous read from the device.
- Returns a Promise containing a Buffer or the Promise will reject upon error.
- This can only be used when `device.on('data', () => {})` is not being used. It will fail if a data handler is registered

### `device.sendFeatureReport(data)`

- `data` - data of HID feature report, with 0th byte being report_id (`[report_id,...]`)
- Returns number of bytes actually written

### `device.getFeatureReport(report_id, report_length)`

- `report_id` - HID feature report id to get
- `report_length` - length of report

### `device.setNonBlocking(no_block)`

- `no_block` - boolean. Set to `true` to enable non-blocking reads
- exactly mirrors `hid_set_nonblocking()` in [`hidapi`](https://github.com/libusb/hidapi)


## Complete Sync API

### `devices = HID.devices()`

- Return array listing all connected HID devices

### `devices = HID.devices(vid,pid)`

- Return array listing all connected HID devices with specific VendorId and ProductId

### `HID.setDriverType(type)`
  - Linux only
  - Sets underlying HID driver type
  - `type` can be `"hidraw"` or `"libusb"`, defaults to `"hidraw"`

### `device = new HID.HID(path,options?:{nonExclusive?:boolean})`

- Open a HID device at the specified platform-specific path

### `device = new HID.HID(vid,pid,options?:{nonExclusive?:boolean})`

- Open first HID device with specific VendorId and ProductId

### `device.on('data', function(data) {} )`

- `data` - Buffer - the data read from the device

### `device.on('error', function(error) {} )`

- `error` - The error Object emitted

### `device.write(data)`

- `data` - the data to be synchronously written to the device,
first byte is Report Id or 0x00 if not using numbered reports.
- Returns number of bytes actually written

### `device.close()`

- Closes the device. Subsequent reads will raise an error.

### `device.pause()`

- Pauses reading and the emission of `data` events.  
This means the underlying device is _silenced_ until resumption --
it is not like pausing a stream, where data continues to accumulate.

### `device.resume()`

- This method will cause the HID device to resume emmitting `data` events.
If no listeners are registered for the `data` event, data will be lost.

- When a `data` event is registered for this HID device, this method will
be automatically called.

### `device.read(callback)`

- Low-level function call to initiate an asynchronous read from the device.
- `callback` is of the form `callback(err, data)`

### `device.readSync()`

- Return an array of numbers data. If an error occurs, an exception will be thrown.

### `device.readTimeout(time_out)`

- `time_out` - timeout in milliseconds
- Return an array of numbers data. If an error occurs, an exception will be thrown.

### `device.sendFeatureReport(data)`

- `data` - data of HID feature report, with 0th byte being report_id (`[report_id,...]`)
- Returns number of bytes actually written

### `device.getFeatureReport(report_id, report_length)`

- `report_id` - HID feature report id to get
- `report_length` - length of report

### `device.setNonBlocking(no_block)`

- `no_block` - boolean. Set to `true` to enable non-blocking reads
- exactly mirrors `hid_set_nonblocking()` in [`hidapi`](https://github.com/libusb/hidapi)

-----

## General notes:

### Thread safety, Worker threads, Context-aware modules
In general `node-hid` is not thread-safe because the underlying C-library it wraps (`hidapi`) is not thread-safe.
To mitigate this we are doing locking to ensure operations are performed safely. If you are using the sync api from multiple worker_threads, this will result in them waiting on each other at times.

### Devices `node-hid` cannot read
The following devices are unavailable to `node-hid` because the OS owns them:

- Keyboards
- Mice
- Barcode readers (in USB HID keyboard mode)
- RFID scanners (in USB HID keyboard mode)
- Postage Scales (in USB HID keyboard mode)

Most OSes will prevent USB HID keyboards or mice, or devices that appear as a keyboard to the OS.
This includes many RFID scanners, barcode readers, USB HID scales, and many other devices.
This is a security precaution. Otherwise, it would be trivial to build keyloggers.

Some keyboard-pretending devices like barcode or RFID readers can be configured to be in
"HID data" mode or "Serial / UART" mode.  If in "HID Data" mode then `node-hid` can access them,
if in "Serial / UART" mode, you should use `node-serialport` instead.

## Mac notes
See General notes above Keyboards with the exception if you open a device in nonExclusive mode.
In that case the os will request permission from the user to read the device.
If permission is given `node-hid` will be able to read data coming from the device normally.

## Windows notes
See General notes above about Keyboards

### Xbox 360 Controller on Windows 10
For reasons similar to mice & keyboards it appears you can't access this controller on Windows 10.



## Linux notes
See General notes above about Keyboards

### udev device permissions
Most Linux distros use `udev` to manage access to physical devices,
and USB HID devices are normally owned by the `root` user.
To allow non-root access, you must create a udev rule for the device,
based on the devices vendorId and productId.

This rule is a text file placed in `/etc/udev/rules.d`.  

For an example HID device (say a blink(1) light with vendorId = 0x27b8 and productId = 0x01ed,
the rules file to support both `hidraw` and `libusb` would look like:
```
SUBSYSTEM=="input", GROUP="input", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="27b8", ATTRS{idProduct}=="01ed", MODE:="666", GROUP="plugdev"
KERNEL=="hidraw*", ATTRS{idVendor}=="27b8", ATTRS{idProduct}=="01ed", MODE="0666", GROUP="plugdev"
```
Note that the values for idVendor and idProduct must be in hex and lower-case.

Save this file as `/etc/udev/rules.d/51-blink1.rules`, unplug the HID device,
and reload the rules with:
```
sudo udevadm control --reload-rules
```

For a complete example, see the
[blink1 udev rules](https://github.com/todbot/blink1/blob/master/linux/51-blink1.rules).


### Selecting driver type

By default as of `node-hid@0.7.0`, the [hidraw](https://www.kernel.org/doc/Documentation/hid/hidraw.txt) driver is used to talk to HID devices. Before `node-hid@0.7.0`, the more older but less capable [libusb](http://libusb.info/) driver was used.  With `hidraw` Linux apps can now see `usage` and `usagePage` attributes of devices.

If you would still like to use the `libusb` driver, then you can do either:

During runtime, you can use `HID.setDriverType('libusb')` immediately after require()-ing `node-hid`:
```js
var HID = require('node-hid');
HID.setDriverType('libusb');
```

If you must have the libusb version and cannot use `setDriverType()`,
you can install older node-hid or build from source:
```
npm install node-hid@0.5.7
```
or:
```
npm install node-hid --build-from-source --driver=libusb
```


## Compiling from source

To compile & develop locally or if `prebuild` cannot download a pre-built
binary for you, you will need the following compiler tools and libraries:

### Linux (kernel 2.6+) : (install examples shown for Debian/Ubuntu)
  * Compilation tools: `apt install build-essential git pkg-config`
  * libudev-dev: `apt install libudev-dev` (Debian/Ubuntu) /
    `yum install libusbx-devel` (Fedora)
  * libusb-1.0-0 w/headers:`apt install libusb-1.0-0 libusb-1.0-0-dev`

### FreeBSD
  * Compilation tools: `pkg install git gcc gmake libiconv node npm`

### Mac OS X 10.8+
  * [Xcode](https://itunes.apple.com/us/app/xcode/id497799835?mt=12)

### Windows 7, 8, 10
  The below is slightly stale. The 2021 solution is to use the official NodeJs Windows installer
  and pick "install native module tools"
  * Visual C++ compiler and Python 2.7
      * either:
        * `npm install --global windows-build-tools`
        * add `%USERPROFILE%\.windows-build-tools\python27` to `PATH`,
         like PowerShell: `$env:Path += ";$env:USERPROFILE\.windows-build-tools\python27"`
      * or:
        * [Python 2.7](https://www.python.org/downloads/windows/)
        * [Visual Studio Express 2013 for Desktop](https://www.visualstudio.com/downloads/download-visual-studio-vs#d-2013-express)


### Building `node-hid` from source, for your projects

```
npm install node-hid --build-from-source
```

### Build `node-hid` for `node-hid` development

* check out a copy of this repo
* change into its directory
* update the submodules
* build the node package

For example:

```
git clone https://github.com/node-hid/node-hid.git
cd node-hid                                        # must change into node-hid directory
npm install -g rimraf                              # just so it doesn't get 'clean'ed
npm run prepublishOnly                             # get the needed hidapi submodule
npm install --build-from-source                    # rebuilds the module with C code
npm run showdevices                                # list connected HID devices
node ./src/show-devices.js                         # same as above
```

You may see some warnings from the C compiler as it compiles
[hidapi](https://github.com/libusb/hidapi) (the underlying C library `node-hid` uses).  
This is expected.

For ease of development, there are also the scripts:
```
npm run gypclean      # "node-gyp clean" clean gyp build directory
npm run gypconfigure  # "node-gyp configure" configure makefiles
npm run gypbuild      # "node-gyp build" build native code
```

### Building `node-hid` for cross-compiling
When cross-compiling you need to override `node-hid`'s normal behavior
of using Linux `pkg-config` to determine CLFAGS and LDFLAGS for `libusb`.
To do this, you can use the `node-gyp` variable `node_hid_no_pkg_config`
and then invoke a `node-hid` rebuild with either:
```
  node-gyp rebuild --node_hid_no_pkg_config=1
```
or
```
  npm gyprebuild --node_hid_no_pkg_config=1
```


## Electron projects using `node-hid`

If using `node-hid` with `webpack` or similar bundler, you may need to exclude
`node-hid` and other libraries with native code.  In webpack, you say which
`externals` you have in your `webpack-config.js`:
```
  externals: {
    "node-hid": 'commonjs node-hid'
  }
```

Examples of `node-hid` in Electron:
* [electron-hid-test](https://github.com/todbot/electron-hid-test) - Simple example of using `node-hid`, should track latest Electron release
* [electron-hid-test-erb](https://github.com/todbot/electron-hid-test-erb) - Simple example of using `node-hid` using [electron-react-boilerplate](https://github.com/electron-react-boilerplate/electron-react-boilerplate/)
* [electron-hid-toy](https://github.com/todbot/electron-hid-toy) - Simple example of using `node-hid`, showing packaging and signing
* [Blink1Control2](https://github.com/todbot/Blink1Control2/) - a complete application, using webpack (e.g. see its [webpack-config.js](https://github.com/todbot/Blink1Control2/blob/master/webpack.config.js))


## NW.js projects using `node-hid`

Without knowing much about NW.js, a quick hacky solution that works is:

```
cd my-nwjs-app
npm install node-hid --save
npm install -g nw-gyp
cd node_modules/node-hid
nw-gyp rebuild --target=0.42.3 --arch=x64  // or whatever NW.js version you have
cd ../..
nwjs .
```


## Support

Please use the [node-hid github issues page](https://github.com/node-hid/node-hid/issues)
for support questions and issues.