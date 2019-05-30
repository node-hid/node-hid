# node-hid - Access USB HID devices from Node.js #

[![npm](https://img.shields.io/npm/dm/node-hid.svg?maxAge=2592000)](http://npmjs.com/package/node-hid)
[![Build Status](https://travis-ci.org/node-hid/node-hid.svg?branch=master)](https://travis-ci.org/node-hid/node-hid)
[![Build status](https://ci.appveyor.com/api/projects/status/sqgrud8yufx12dbt?svg=true)](https://ci.appveyor.com/project/todbot/node-hid/branch/master)


* [Platform Support](#platform-support)
   * [Supported Node versions](#supported-node-versions)
   * [Supported Electron versions](#supported-electron-versions)
* [Installation](#installation)
   * [Installation Special Cases](#installation-special-cases)
* [Examples](#examples)
* [Usage](#usage)
   * [List all HID devices connected](#list-all-hid-devices-connected)
     * [Cost of HID.devices() and new HID.HID()](#cost-of-hiddevices-and-new-hidhid-for-detecting-device-plugunplug)
   * [Opening a device](#opening-a-device)
   * [Picking a device from the device list](#picking-a-device-from-the-device-list)
   * [Reading from a device](#reading-from-a-device)
   * [Writing to a device](#writing-to-a-device)
* [Complete API](#complete-api)
   * [devices = HID.devices()](#devices--hiddevices)
   * [HID.setDriverType(type)](#hidsetdrivertypetype)
   * [device = new HID.HID(path)](#device--new-hidhidpath)
   * [device = new HID.HID(vid,pid)](#device--new-hidhidvidpid)
   * [device.on('data', function(data) {} )](#deviceondata-functiondata--)
   * [device.on('error, function(error) {} )](#deviceonerror-functionerror--)
   * [device.write(data)](#devicewritedata)
   * [device.close()](#deviceclose)
   * [device.pause()](#devicepause)
   * [device.resume()](#deviceresume)
   * [device.read(callback)](#devicereadcallback)
   * [device.readSync()](#devicereadsync)
   * [device.readTimeout(time_out)](#devicereadtimeouttime_out)
   * [device.sendFeatureReport(data)](#devicesendfeaturereportdata)
   * [device.getFeatureReport(report_id, report_length)](#devicegetfeaturereportreport_id-report_length)
* [Windows notes](#windows-notes)
   * [Mice and keyboards](#mice-and-keyboards)
   * [Xbox 360 Controller on Windows 10](#xbox-360-controller-on-windows-10)
   * [Prepend byte to hid_write()](#prepend-byte-to-hid_write)
* [Linux notes](#linux-notes)
   * [Selecting driver type](#selecting-driver-type)
   * [udev device permissions](#udev-device-permissions)
* [Compiling from source](#compiling-from-source)
   * [To build node-hid from source for your project:](#to-build-node-hid-from-source-for-your-project)
   * [To build node-hid for development:](#to-build-node-hid-for-development)
* [Using node-hid in Electron projects](#using-node-hid-in-electron-projects)
* [Using node-hid in NW.js projects](#using-node-hid-in-nwjs-projects)
* [Support](#support)



## Platform Support
`node-hid` supports Node.js v6 and upwards. For versions before that,
you will need to build from source. The platforms, architectures and node versions `node-hid` supports are the following.
In general we try to provide pre-built native library binaries for the most common platforms, Node and Electron versions.

We strive to make `node-hid` cross-platform so there's a good chance any
combination not listed here will compile and work.

### Supported Platofrms ###
- Windows x86 (32-bit) (¹)
- Windows x64 (64-bit)
- Mac OSX 10.9+
- Linux x64 (²)
- Linux x86 (¹)
- Linux ARM / Raspberry Pi (¹)
- Linux MIPSel (¹)
- Linux PPC64 (¹)

¹ prebuilt-binaries not provided for these platforms
² prebuilt binary built on Ubuntu 16.04 x64

### Supported Node versions ###

* Node v6 to
* Node v12

### Supported Electron versions ###

* Electron v1 to (³)
* Electron v5


³ Electron v1.8 currently has issues, but prebuilt binaries are provided.


## Installation

For most "standard" use cases (node v4.x on mac, linux, windows on a x86 or x64 processor), `node-hid` will install nice and easy with a standard:

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

We are using [prebuild](https://github.com/mafintosh/prebuild) to compile and post binaries of the library for most common use cases (linux, mac, windows on standard processor platforms). If you are on a special case, `node-hid` will work, but it will compile the binary when you install.

If `node-hid` doesn't have a pre-built binary for your system
(e.g. Linux on Raspberry Pi),
`node-gyp` is used to compile `node-hid` locally.  It will need the pre-requisites
listed in [Compling from source](#compiling-from-source) below.


## Examples

In the `src/` directory, various JavaScript programs can be found
that talk to specific devices in some way.  Some interesting ones:
- [`show-devices.js`](./src/show-devices.js) - display all HID devices in the system
- [`test-ps3-rumbleled.js`](./src/test-ps3-rumbleled.js) - Read PS3 joystick and control its LED & rumblers
- [`test-powermate.js`](./src/test-powermate.js) - Read Griffin PowerMate knob and change its LED
- [`test-blink1.js`](./src/test-blink1.js) - Fade colors on blink(1) RGB LED
- [`test-bigredbutton.js`](./src/test-bigredbutton.js) - Read Dreamcheeky Big Red Button
- [`test-teensyrawhid.js`](./src/test-teensyrawhid.js) - Read/write Teensy running RawHID "Basic" Arduino sketch

To try them out, run them like `node src/showdevices.js` from within the node-hid directory.

----

## Usage

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

#### Cost of `HID.devices()` and `new HID.HID()` for detecting device plug/unplug
Both `HID.devices()` and `new HID.HID()` are relatively costly, each causing a USB (and potentially Bluetooth) enumeration. This takes time and OS resources. Doing either can slow down the read/write that you do in parallel with a device, and cause other USB devices to slow down too. This is how USB works.

If you are polling `HID.devices()` or doing repeated `new HID.HID(vid,pid)` to detect device plug / unplug, consider instead using [node-usb-detection](https://github.com/MadLittleMods/node-usb-detection). `node-usb-detection` uses OS-specific, non-bus enumeration ways to detect device plug / unplug.

### Opening a device

Before a device can be read from or written to, it must be opened.
The `path` can be determined by a prior HID.devices() call.
Use either the `path` from the list returned by a prior call to `HID.devices()`:

```js
var device = new HID.HID(path);
```

or open the first device matching a VID/PID pair:

```js
var device = new HID.HID(vid,pid);
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

```js
device.write([0x00, 0x01, 0x01, 0x05, 0xff, 0xff]);
```
```js
device.sendFeatureReport( [0x01, 'c', 0, 0xff,0x33,0x00, 70,0, 0] );
```
Notes:
- Both `device.write()` and `device.sendFeatureReport()` return number of bytes written
- Some devices use reportIds for OUTPUT reports.  In that case,
the first byte of the array to `write()` should be the reportId.
- BUG: Windows requires the prepend of an extra byte due to a bug in hidapi (see issue #187 and Windows notes below)


## Complete API

### `devices = HID.devices()`

- Return array listing all connected HID devices

### `HID.setDriverType(type)`
  - Linux only
  - Sets underlying HID driver type
  - `type` can be `"hidraw"` or `"libusb"`, defaults to `"hidraw"`

### `device = new HID.HID(path)`

- Open a HID device at the specified platform-specific path

### `device = new HID.HID(vid,pid)`

- Open first HID device with specific VendorId and ProductId

### `device.on('data', function(data) {} )`

- `data` - Buffer - the data read from the device

### `device.on('error, function(error) {} )`

- `error` - The error Object emitted

### `device.write(data)`

- `data` - the data to be synchronously written to the device
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

-----

## Windows notes

### Mice and keyboards
In general you cannot access USB HID keyboards or mice.  
The OS owns these devices.

### Xbox 360 Controller on Windows 10
For reasons similar to mice & keyboards it appears you can't access this controller on Windows 10.

### Prepend byte to `hid_write()`
Because of a limitation in the underlying `hidapi` library, if you are using `hid_write()` you should prepend a byte to any data buffer, e.g.
```js
var device = new HID.HID(vid,pid);
var buffer = Array(64).fill(0x33); // device has 64-byte report
if(os.platform === 'win32') {
  buffer.unshift(0);  // prepend throwaway byte
}
```


## Linux notes

### Selecting driver type

By default as of `node-hid@0.7.0`, the [hidraw](https://www.kernel.org/doc/Documentation/hid/hidraw.txt) driver is used to talk to HID devices. Before `node-hid@0.7.0`, the more older but less capable [libusb](http://libusb.info/) driver was used.  With `hidraw` Linux apps can now see `usage` and `usagePage` attributes of devices.

If you would still like to use the `libusb` driver, then you can do either:
```
npm install node-hid@0.5.7
```
or:
```
npm install node-hid --build-from-source --driver=libusb
```

Or during runtime, you can use `HID.setDriverType('libusb')` immediately after require()-ing `node-hid`:
```js
var HID = require('node-hid');
HID.setDriverType('libusb');
```




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


## Compiling from source

To compile & develop locally or if `prebuild` cannot download a pre-built
binary for you, you will need the following tools:

* All OSes:
    * `node-gyp` installed globally: `npm install -g node-gyp`

* Linux (kernel 2.6+) : install examples shown for Ubuntu
    * Compilation tools: `apt install build-essential git`
    * gcc-4.8+: `apt install gcc-4.8 g++-4.8 && export CXX=g++-4.8`
    * libusb-1.0-0 w/headers:`sudo apt install libusb-1.0-0 libusb-1.0-0-dev`
    * libudev-dev: (Fedora only) `yum install libusbx-devel`
* Mac OS X 10.8+
    * [Xcode](https://itunes.apple.com/us/app/xcode/id497799835?mt=12)
* Windows XP, 7, 8, 10
    * Visual C++ compiler and Python 2.7
        * either:
            * `npm install --global windows-build-tools`
            * add `%USERPROFILE%\.windows-build-tools\python27` to `PATH`,
           like PowerShell: `$env:Path += ";$env:USERPROFILE\.windows-build-tools\python27"`
        * or:
          * [Python 2.7](https://www.python.org/downloads/windows/)
          * [Visual Studio Express 2013 for Desktop](https://www.visualstudio.com/downloads/download-visual-studio-vs#d-2013-express)


### To build `node-hid` from source for your project:

```
npm install node-hid --build-from-source
```

### To build `node-hid` for development:

* check out a copy of this repo
* change into its directory
* update the submodules
* build the node package

For example:

```
git clone https://github.com/node-hid/node-hid.git
cd node-hid                                        # must change into node-hid directory
npm run prepublish                                 # get the needed hidapi submodule
npm install --build-from-source                    # rebuilds the module with C code
node ./src/show-devices.js
```

You will see some warnings from the C compiler as it compiles
[hidapi](https://github.com/signal11/hidapi) (the underlying C library `node-hid` uses).  
This is expected.

For ease of development, there are also the scripts:
```
npm run gypclean      # "node-gyp clean" clean gyp build directory
npm run gypconfigure  # "node-gyp configure" configure makefiles
npm run gypbuild      # "node-gyp build" build native code
```

## Using `node-hid` in Electron projects
In your electron project, add `electron-rebuild` to your `devDependencies`.
Then in your package.json `scripts` add:

```
  "postinstall": "electron-rebuild --force"
```
This will cause `npm` to rebuild `node-hid` for the version of Node that is in Electron.
If you get an error similar to `The module "HID.node" was compiled against a differnt version of Node.js`
then `electron-rebuild` hasn't been run and Electron is trying to use `node-hid` not built for it.

If you want a specific version of electron, do something like:

```
  "postinstall": "electron-rebuild -v 0.36.5 --force -m . -w node-hid"
```

If using `node-hid` with `webpack`, you may find it useful to list `node-hid` as an external in your `webpack-config.js`:
```
  externals: {
    "node-hid": 'commonjs node-hid'
  }
```
(You can see an example of this in [Blink1Control2](https://github.com/todbot/Blink1Control2/)'s [webpack-config.js](https://github.com/todbot/Blink1Control2/blob/master/webpack.config.js)

## Using `node-hid` in NW.js projects
(TBD)

## Support

Please use the [node-hid github issues page](https://github.com/node-hid/node-hid/issues)
for support questions and issues.
