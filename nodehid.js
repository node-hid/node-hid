
var os = require('os')

var EventEmitter = require("events").EventEmitter,
    util = require("util");

var driverType = null;
function setDriverType(type) {
    driverType = type;
}

// lazy load the C++ binding
var binding = null;
function loadBinding() {
    if( !binding ) {
        if( os.platform() === 'linux' ) {
            // Linux defaults to hidraw
            if( !driverType || driverType === 'hidraw' ) {
                binding = require('bindings')('HID_hidraw.node');
            } else {
                binding = require('bindings')('HID.node');
            }
        }
        else {
            binding = require('bindings')('HID.node');
        }
    }
}

//This class is a wrapper for `binding.HID` class
function HID() {
    if (!new.target) {
        throw new Error('HID() must be called with \'new\' operator');
    }

    EventEmitter.call(this);

    // Check if an instance already exists in the cache
    if (HID.cachedInstance) {
        return HID.cachedInstance;
    }

    loadBinding();

    var thisPlusArgs = new Array(arguments.length + 1);
    thisPlusArgs[0] = null;
    for(var i = 0; i < arguments.length; i++)
        thisPlusArgs[i + 1] = arguments[i];

    this._raw = new (Function.prototype.bind.apply(binding.HID, thisPlusArgs))();

    // Cache this instance for future calls
    HID.cachedInstance = this;

    for(var key in binding.HID.prototype)
        this[key] = binding.HID.prototype[key].bind(this._raw);

    this._paused = true;
    var self = this;
    self.on("newListener", function(eventName, listener) {
        if(eventName == "data")
            process.nextTick(self.resume.bind(self));
    });
}

//Inherit prototype methods
util.inherits(HID, EventEmitter);
//Don't inherit from `binding.HID`; that's done above already!

HID.prototype.close = function close() {
    this._closing = true;
    this.removeAllListeners();
    this._raw.close();
    this._closed = true;
};
//Pauses the reader, which stops "data" events from being emitted
HID.prototype.pause = function pause() {
    this._paused = true;
};

HID.prototype.read = function read(callback) {
    if (this._closed) {
    throw new Error('Unable to read from a closed HID device');
  } else {
    return this._raw.read(callback);
  }
};

HID.prototype.resume = function resume() {
    var self = this;
    if(self._paused && self.listeners("data").length > 0)
    {
        //Start polling & reading loop
        self._paused = false;
        self.read(function readFunc(err, data) {
            if(err)
            {
                //Emit error and pause reading
                self._paused = true;
                if(!self._closing)
                    self.emit("error", err);
                //else ignore any errors if I'm closing the device
            }
            else
            {
                //If there are no "data" listeners, we pause
                if(self.listeners("data").length <= 0)
                    self._paused = true;
                //Keep reading if we aren't paused
                if(!self._paused)
                    self.read(readFunc);
                //Now emit the event
                self.emit("data", data);
            }
        });
    }
};

function showdevices() {
    loadBinding();
    return binding.devices.apply(HID,arguments);
}

// Static property for caching the instance
HID.cachedInstance = null;

//Expose API
exports.HID = HID;
exports.devices = showdevices;
exports.setDriverType = setDriverType;
