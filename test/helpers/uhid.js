/**
 * Minimal userspace driver for /dev/uhid, enough to host a fake HID device.
 *
 * All structs are __packed__ little-endian (see /usr/include/linux/uhid.h).
 * struct uhid_event = u32 type + union payload at offset 4; the largest member
 * (uhid_create2_req) makes sizeof(uhid_event) = 4376. Reads MUST supply the full
 * event size (the kernel returns EINVAL on short reads); short writes are fine.
 */
const fs = require("fs");
const EventEmitter = require("events");

const EVENT_SIZE = 4376;
const DATA_MAX = 4096;

const EventType = {
    DESTROY: 1,
    START: 2,
    STOP: 3,
    OPEN: 4,
    CLOSE: 5,
    OUTPUT: 6,
    GET_REPORT: 9,
    GET_REPORT_REPLY: 10,
    CREATE2: 11,
    INPUT2: 12,
    SET_REPORT: 13,
    SET_REPORT_REPLY: 14,
};

const ReportType = {
    FEATURE: 0,
    OUTPUT: 1,
    INPUT: 2,
};

const BUS_USB = 0x03;

function encodeCreate2({ name, phys = "", uniq, vendorId, productId, version = 0, country = 0, reportDescriptor }) {
    if (reportDescriptor.length > DATA_MAX) throw new Error("report descriptor too long");

    const buf = Buffer.alloc(280 + reportDescriptor.length);
    buf.writeUInt32LE(EventType.CREATE2, 0);
    buf.write(name, 4, 127, "utf8");
    buf.write(phys, 132, 63, "utf8");
    buf.write(uniq, 196, 63, "utf8");
    buf.writeUInt16LE(reportDescriptor.length, 260);
    buf.writeUInt16LE(BUS_USB, 262); // hidapi's hidraw backend ignores devices on other buses
    buf.writeUInt32LE(vendorId, 264);
    buf.writeUInt32LE(productId, 268);
    buf.writeUInt32LE(version, 272);
    buf.writeUInt32LE(country, 276);
    reportDescriptor.copy(buf, 280);
    return buf;
}

function encodeInput2(data) {
    if (data.length > DATA_MAX) throw new Error("input report too long");

    const buf = Buffer.alloc(6 + data.length);
    buf.writeUInt32LE(EventType.INPUT2, 0);
    buf.writeUInt16LE(data.length, 4);
    data.copy(buf, 6);
    return buf;
}

function encodeGetReportReply(id, err, data) {
    const buf = Buffer.alloc(12 + data.length);
    buf.writeUInt32LE(EventType.GET_REPORT_REPLY, 0);
    buf.writeUInt32LE(id, 4);
    buf.writeUInt16LE(err, 8);
    buf.writeUInt16LE(data.length, 10);
    data.copy(buf, 12);
    return buf;
}

function encodeSetReportReply(id, err) {
    const buf = Buffer.alloc(10);
    buf.writeUInt32LE(EventType.SET_REPORT_REPLY, 0);
    buf.writeUInt32LE(id, 4);
    buf.writeUInt16LE(err, 8);
    return buf;
}

function encodeDestroy() {
    const buf = Buffer.alloc(4);
    buf.writeUInt32LE(EventType.DESTROY, 0);
    return buf;
}

function decodeEvent(buf) {
    const type = buf.readUInt32LE(0);
    switch (type) {
        case EventType.START:
            return { type, devFlags: buf.readBigUInt64LE(4) };
        case EventType.GET_REPORT:
            return { type, id: buf.readUInt32LE(4), rnum: buf.readUInt8(8), rtype: buf.readUInt8(9) };
        case EventType.SET_REPORT: {
            const size = buf.readUInt16LE(10);
            return {
                type,
                id: buf.readUInt32LE(4),
                rnum: buf.readUInt8(8),
                rtype: buf.readUInt8(9),
                data: Buffer.from(buf.subarray(12, 12 + size)),
            };
        }
        case EventType.OUTPUT: {
            const size = buf.readUInt16LE(4100); // note: size comes after the data array
            return { type, rtype: buf.readUInt8(4102), data: Buffer.from(buf.subarray(4, 4 + size)) };
        }
        default:
            return { type };
    }
}

class UhidDevice extends EventEmitter {
    constructor(options) {
        super();
        this.options = options;
        this.fd = null;
        this.started = false;
        this.destroyed = false;
        this.readBuf = Buffer.alloc(EVENT_SIZE);
    }

    async create(timeoutMs = 5000) {
        this.fd = fs.openSync("/dev/uhid", fs.constants.O_RDWR | fs.constants.O_NONBLOCK);
        fs.writeSync(this.fd, encodeCreate2(this.options));
        this._scheduleRead(0);

        await new Promise((resolve, reject) => {
            const timer = setTimeout(() => reject(new Error("timed out waiting for UHID_START")), timeoutMs);
            this.once("start", () => {
                clearTimeout(timer);
                resolve();
            });
        });
        this.started = true;
    }

    _scheduleRead(delayMs) {
        if (this.fd === null) return;
        this.readTimer = setTimeout(() => this._readOnce(), delayMs);
    }

    _readOnce() {
        if (this.fd === null) return;
        fs.read(this.fd, this.readBuf, 0, EVENT_SIZE, null, (err, bytesRead) => {
            if (this.fd === null) return;
            if (err) {
                if (err.code === "EAGAIN") {
                    // Nothing pending. Poll quickly: feature-report ioctls block their caller
                    // until we answer, and the kernel gives us only 5 seconds
                    this._scheduleRead(5);
                } else {
                    this.emit("error", err);
                }
                return;
            }

            if (bytesRead > 0) {
                const ev = decodeEvent(this.readBuf);
                this._dispatch(ev);
            }
            this._scheduleRead(0);
        });
    }

    _dispatch(ev) {
        switch (ev.type) {
            case EventType.START:
                this.emit("start", ev.devFlags);
                break;
            case EventType.STOP:
                this.emit("stop");
                break;
            case EventType.OPEN:
                this.emit("open");
                break;
            case EventType.CLOSE:
                this.emit("close");
                break;
            case EventType.OUTPUT:
                this.emit("output", ev.data, ev.rtype);
                break;
            case EventType.GET_REPORT:
                this.emit("getReport", ev);
                break;
            case EventType.SET_REPORT:
                this.emit("setReport", ev);
                break;
            default:
                // Ignore anything we don't handle (eg legacy events)
                break;
        }
    }

    sendInput(data) {
        fs.writeSync(this.fd, encodeInput2(data));
    }

    replyGetReport(id, err, data) {
        fs.writeSync(this.fd, encodeGetReportReply(id, err, data));
    }

    replySetReport(id, err) {
        fs.writeSync(this.fd, encodeSetReportReply(id, err));
    }

    /** Simulate an unplug. The fd stays open so the process can keep serving RPC */
    destroy() {
        if (this.destroyed || this.fd === null) return;
        this.destroyed = true;
        fs.writeSync(this.fd, encodeDestroy());
    }

    close() {
        if (this.fd === null) return;
        this.destroy();
        clearTimeout(this.readTimer);
        const fd = this.fd;
        this.fd = null;
        fs.closeSync(fd);
    }
}

module.exports = { UhidDevice, EventType, ReportType, EVENT_SIZE };
