# node-hid - Access USB HID devices from Node.js #

[![npm](https://img.shields.io/npm/dm/node-hid.svg?maxAge=2592000)](http://npmjs.com/package/node-hid)
[![Build Status](https://travis-ci.org/node-hid/node-hid.svg?branch=master)](https://travis-ci.org/node-hid/node-hid)
[![Build status](https://ci.appveyor.com/api/projects/status/sqgrud8yufx12dbt?svg=true)](https://ci.appveyor.com/project/todbot/node-hid/branch/master)

  * [Platform Support](#platform-support)
  * [Installation](#installation)
     * [Installation Special Cases](#installation-special-cases)
  * [Examples](#examples)
  * [Usage](#usage)
     * [List all HID devices connected](#list-all-hid-devices-connected)
     * [Opening a device](#opening-a-device)
     * [Reading from a device](#reading-from-a-device)
     * [Writing to a device](#writing-to-a-device)
  * [Complete API](#complete-api)
     * [devices = HID.devices()](#devices--hiddevices)
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
  * [Notes for Specific Devices](#notes-for-specific-devices)
  * [Linux-specific Notes](#linux-specific-notes)
     * [usage and <code>usagePage</code> device info fields](#usage-and-usagepage-device-info-fields)
     * [hidraw support](#hidraw-support)
     * [udev device permissions](#udev-device-permissions)
  * [Compiling from source](#compiling-from-source)
   * [To build node-hid from source for your project:](#to-build-node-hid-from-source-for-your-project)
   * [To build node-hid for development:](#to-build-node-hid-for-development)
  * [Using node-hid in Electron projects](#using-node-hid-in-electron-projects)
  * [Using node-hid in NW.js projects](#using-node-hid-in-nwjs-projects)
  * [Support](#support)

## Platform Support
`node-hid` supports Node.js v4 and upwards. For versions 0.10 and 0.12,
you will need to build from source. The platforms, architectures and node versions `node-hid` supports are the following.
Those with checks we provide pre-built binaries, for the others you will need to compile.

| Platform / Arch | Node v4.x | Node v6.x | Node v7.x | Node v8.x | Electron v1.0.2 | Electron v1.2.8 | Electron v1.3.13 | Electron v1.4.15 | Electron v1.5.0 | Electron v1.6.0 | Electron v1.7.0 |
|       ---       | --- | --- | --- | --- | --- | --- |--- | --- | --- | --- | --- |
| Windows / x86   |  ☑  |  ☑  |  ☑  |  ☑  |  ☑  |  ☑  | ☑  |  ☑  |  ☑  |  ☑  |  ☑  |
| Windows / x64   |  ☑  |  ☑  |  ☑  |  ☑  |  ☑  |  ☑  | ☑  |  ☑  |  ☑  |  ☑  |  ☑  |
| Mac OSX / x64   |  ☑  |  ☑  |  ☑  |  ☑  |  ☑  |  ☑  | ☑  |  ☑  |  ☑  |  ☑  |  ☑  |
| Linux / x64     |  ☑  |  ☑  |  ☑  |  ☑  |  ☑  |  ☑  | ☑  |  ☑  |  ☑  |  ☑  |  ☑  |
| Linux / ia32¹   |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  | ☐  |  ☐  |  ☐  |  ☐  |  ☐  |
| Linux / ARM v6¹ |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  | ☐  |  ☐  |  ☐  |  ☐  |  ☐  |
| Linux / ARM v7¹ |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  | ☐  |  ☐  |  ☐  |  ☐  |  ☐  |
| Linux / ARM v8¹ |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  | ☐  |  ☐  |  ☐  |  ☐  |  ☐  |
| Linux / MIPSel¹ |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  | ☐  |  ☐  |  ☐  |  ☐  |  ☐  |
| Linux / PPC64¹  |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  |  ☐  | ☐  |  ☐  |  ☐  |  ☐  |  ☐  |

¹ ia32, ARM, MIPSel and PPC64 platforms are known to work but are not currently part of our test or build matrix.  ARM v4 and v5 was dropped from Node.js after Node v0.10.

## Installation

For most "standard" use cases (node v4.x on mac, linux, windows on a x86 or x64 processor), `node-hid` will install nice and easy with a standard:

```
npm install node-hid
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

To try them out, call them like `node src/showdevices.js` from the node-hid directory.

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

### Reading from a device

A `node-hid` device is an EventEmitter.
Reading from a device is performed by registering a "data" event handler:

```js
device.on("data", function(data) {});
```

You can also listen for errors like this:

```js
device.on("error", function(err) {});
```
Notes:
- All reading is asynchronous
- The `data` event receives INPUT reports. To receive Feature reports,
  see the `readFeatureReport()` method below.
- To remove an event handler, close the device with `device.close()`


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
- BUG: if the first byte of a `write()` is 0x00, you may need to prepend an extra 0x00 due to a bug in hidapi (see issue #187)


## Complete API

### `devices = HID.devices()`

- Return array listing all connected HID devices

### `device = new HID.HID(path)`

- Open a HID device at the specifed platform-speific path

### `device = new HID.HID(vid,pid)`

- Open first HID device with speciic VendorId and ProductId

### `device.on('data', function(data) {} )`

- `data` - Buffer - the data read from the device

### `device.on('error, function(error) {} )`

- `error` - The error Object emitted

### `device.write(data)`

- `data` - the data to be synchronously written to the device

### `device.close()`

Closes the device. Subsequent reads will raise an error.

### `device.pause()`

Pauses reading and the emission of `data` events.

### `device.resume()`

This method will cause the HID device to resume emmitting `data` events.
If no listeners are registered for the `data` event, data will be lost.

When a `data` event is registered for this HID device, this method will
be automatically called.

### `device.read(callback)`

Low-level function call to initiate an asynchronous read from the device.
`callback` is of the form `callback(err, data)`

### `device.readSync()`

Return an array of numbers data. If an error occurs, an exception will be thrown.

### `device.readTimeout(time_out)`

- `time_out` - timeout in milliseconds
Return an array of numbers data. If an error occurs, an exception will be thrown.

### `device.sendFeatureReport(data)`
- `data` - data of HID feature report, with 0th byte being report_id (`[report_id,...]`)

### `device.getFeatureReport(report_id, report_length)`
- `report_id` - HID feature report id to get
- `report_length` - length of report


## Notes for Specific Devices

- Mouse and keyboards on Windows -- does not work, the OS will not allow it
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

## Using `node-hid` in NW.js projects
(TBD)

## Support

Please use the [node-hid github issues page](https://github.com/node-hid/node-hid/issues)
for support questions and issues.
