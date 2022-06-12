
const EventEmitter = require("events").EventEmitter;
const util = require("util");

let driverType = null;
function setDriverType(type) {
    driverType = type;
}

// lazy load the C++ binding
let binding = null;
function loadBinding() {
    if( !binding ) {
        if( process.platform === 'linux' ) {
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
    //Inherit from EventEmitter
    EventEmitter.call(this);

    loadBinding();

    /* We also want to inherit from `binding.HID`, but unfortunately,
        it's not so easy for native Objects. For example, the
        following won't work since `new` keyword isn't used:

        `binding.HID.apply(this, arguments);`

        So... we do this craziness instead...
    */
    var thisPlusArgs = new Array(arguments.length + 1);
    thisPlusArgs[0] = null;
    for(var i = 0; i < arguments.length; i++)
        thisPlusArgs[i + 1] = arguments[i];
    this._raw = new (Function.prototype.bind.apply(binding.HID,
        thisPlusArgs) )();

    /* Now we have `this._raw` Object from which we need to
        inherit.  So, one solution is to simply copy all
        prototype methods over to `this` and binding them to
        `this._raw`
    */
    for(var i in binding.HID.prototype)
        this[i] = binding.HID.prototype[i].bind(this._raw);

    /* We are now done inheriting from `binding.HID` and EventEmitter.

        Now upon adding a new listener for "data" events, we start
        polling the HID device using `read(...)`
        See `resume()` for more details. */
    var self = this;
    self.on("newListener", function(eventName, listener) {
        if(eventName == "data")
            process.nextTick(self.resume.bind(self) );
    });
    self.on("removeListener", function(eventName, listener) {
        if(eventName == "data" && self.listenerCount("data") == 0)
            process.nextTick(self.pause.bind(self) );
    })
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
    this._raw.readStop();
};

HID.prototype.resume = function resume() {
    var self = this;
    if(self.listenerCount("data") > 0)
    {
        //Start polling & reading loop
        self._raw.readStart(function (err, data) {
            if (err) {
                if(!self._closing)
                    self.emit("error", err);
                //else ignore any errors if I'm closing the device
            } else {
                self.emit("data", data);
            }
        }, self)
    }
};

class HIDAsync extends EventEmitter {
    constructor(raw) {
        super()

        this._raw = raw

        /* Now we have `this._raw` Object from which we need to
            inherit.  So, one solution is to simply copy all
            prototype methods over to `this` and binding them to
            `this._raw`.
            We explicitly wrap them in an async method, to ensure 
            that any thrown errors are promise rejections
        */
        for (let i in this._raw) {
            this[i] = async (...args) => this._raw[i](...args);
        }

        /* Now upon adding a new listener for "data" events, we start
            the read thread executing. See `resume()` for more details.
        */
        this.on("newListener", (eventName, listener) =>{
            if(eventName == "data")
                process.nextTick(this.resume.bind(this) );
        });
        this.on("removeListener", (eventName, listener) => {
            if(eventName == "data" && this.listenerCount("data") == 0)
                process.nextTick(this.pause.bind(this) );
        })
    }

    async close() {
        this._closing = true;
        this.removeAllListeners();
        await this._raw.close();
        this._closed = true;
    }
    
    //Pauses the reader, which stops "data" events from being emitted
    pause() {
        this._raw.readStop();
    }

    resume() {
        if(this.listenerCount("data") > 0)
        {
            //Start polling & reading loop
            this._raw.readStart((err, data) => {
                if (err) {
                    if(!this._closing)
                        this.emit("error", err);
                    //else ignore any errors if I'm closing the device
                } else {
                    this.emit("data", data);
                }
            })
        }
    }
}

function showdevices() {
    loadBinding();
    return binding.devices.apply(HID,arguments);
}

function showdevicesAsync() {
    loadBinding();
    return binding.devicesAsync.apply(HID,arguments);
}

async function openAsyncHIDDevice() {
    loadBinding();
    const native = await binding.openAsyncHIDDevice.apply(HID, arguments);
    return new HIDAsync(native)
}

//Expose API
exports.HID = HID;
exports.devices = showdevices;
exports.devicesAsync = showdevicesAsync;
exports.openAsyncHIDDevice = openAsyncHIDDevice;
exports.setDriverType = setDriverType;
