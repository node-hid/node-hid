
const EventEmitter = require("events").EventEmitter;
const util = require("util");

let driverType = null;
function setDriverType(type) {
    driverType = type;
}

// lazy load the C++ binding
let binding = null;
function loadBinding() {
    if (!binding) {
        const options = require('./binding-options');
        if (process.platform === "linux" && (!driverType || driverType === "hidraw")) {
            options.name = 'HID_hidraw';
        }
        binding = require("pkg-prebuilds/bindings")(__dirname, options);
    }
}

//This class is a wrapper for `binding.HID` class
function HID() {

    // see issue #150 (enhancement, solves issue #149)
    // throw an error for those who forget to instantiate, i.e. by "*new* HID.HID()"
    // and who would otherwise be left trying to figure out why "self.on is not a function"
    if (!new.target) {
        throw new Error('HID() must be called with \'new\' operator');
    }

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
    this._paused = true;
    var self = this;
    self.on("newListener", function(eventName, listener) {
        if(eventName == "data")
            process.nextTick(self.resume.bind(self) );
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
            try {
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
            } catch (e) {
                // Emit an error on the device instead of propagating to a c++ exception
                setImmediate(() => {
                    this.emit("error", e);
                });
            }
        });
    }
};

class HIDAsync extends EventEmitter {
    constructor(raw) {
        super()

        if (!(raw instanceof binding.HIDAsync)) {
            throw new Error(`HIDAsync cannot be constructed directly. Use HIDAsync.open() instead`)
        }

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

    static async open(...args) {
        loadBinding();
        const native = await binding.openAsyncHIDDevice(...args);
        return new HIDAsync(native)
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
                try {
                    if (err) {
                        if(!this._closing)
                            this.emit("error", err);
                        //else ignore any errors if I'm closing the device
                    } else {
                        this.emit("data", data);
                    }
                } catch (e) {
                    // Emit an error on the device instead of propagating to a c++ exception
                    setImmediate(() => {
                        this.emit("error", e);
                    });
                }
            })
        }
    }
}

function showdevices() {
    loadBinding();
    return binding.devices.apply(HID,arguments);
}

function showdevicesAsync(...args) {
    loadBinding();
    return binding.devicesAsync(...args);
}


//Expose API
exports.HID = HID;
exports.HIDAsync = HIDAsync;
exports.devices = showdevices;
exports.devicesAsync = showdevicesAsync;
exports.setDriverType = setDriverType;
