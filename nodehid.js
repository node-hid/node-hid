var EventEmitter = require("events").EventEmitter,
    util = require("util");

//Load C++ binding
//var binding = require("./build/Release/HID.node");
var binary = require('node-pre-gyp');
var path = require('path');
var binding_path = binary.find(path.resolve(path.join(__dirname,'./package.json')));
var binding = require(binding_path);

//This class is a wrapper for `binding.HID` class
function HID() {
    //Inherit from EventEmitter
    EventEmitter.call(this);

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
        `this._raw` if we don't already have a wrapper implemented
    */
    for (var i in binding.HID.prototype) {
		if (!HID.prototype[i]) {
			this[i] = (function(fn){
				return function(){
					var args = [].slice.call(arguments);
					console.log(fn, 'called with', args);
					this._raw[fn].apply(this._raw, args);
				};
			})(i);
		}
	}

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

HID.prototype.write = function write(data) {
    if (this._closed) {
		throw new Error('Unable to write to a closed HID device');
	}

	return this._raw.write(toBuffer(data));
};

HID.prototype.sendFeatureReport = function sendFeatureReport(data) {
	if (this._closed) {
		throw new Error('Unable to sendFeatureReport to a closed HID device');
	}

	return this._raw.sendFeatureReport(toBuffer(data));
};

// utility method to convert an Array or Buffer to a Buffer for the native C++
// code to work with.
function toBuffer (data) {
	if (Buffer.isBuffer(data)) {
		return data;
	}
	if (Array.isArray(data)) {
		return data.reduce(function(buf, value, i){
			// make sure value is a number that can be converted to a byte
			if (value >= 0 && value < 256) {
				buf.writeUInt8(value, i);
				return buf;
			}
			throw new Error("Unexpected array element in array to send/write, expecting only integers between 0 and 255");
		}, (Buffer.hasOwnProperty(alloc))? Buffer.alloc(data.length) : new Buffer(data.length).fill(0))
	}

	throw new Error("Attempted to write or send unsupported argument to HID.");
}

//Expose API
exports.HID = HID;
exports.devices = binding.devices;
