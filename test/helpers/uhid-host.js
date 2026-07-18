/**
 * Child process hosting the fake blink(1) uhid device.
 *
 * This MUST be a separate process from the tests: the sync feature-report APIs
 * block the caller's event loop inside an ioctl that only returns once this
 * process answers the corresponding uhid GET/SET_REPORT event. In-process, that
 * is a deadlock.
 *
 * The kernel destroys the uhid device when our fd closes and we exit on IPC
 * disconnect, so a dead test runner can never leak virtual devices.
 */
const fs = require("fs");
const path = require("path");

const { UhidDevice, ReportType } = require("./uhid");
const blink1 = require("./blink1-protocol");

let device = null;

const state = {
    version: 204,
    rgb: { r: 0, g: 0, b: 0 },
    fadeMillis: 0,
    ledn: 0,
    lastCommand: null,
    feature1Reply: Buffer.from([blink1.REPORT_ID, 0, 0, 0, 0, 0, 0, 0, 0]),
    feature2Reply: (() => {
        const b = Buffer.alloc(blink1.REPORT2_LENGTH + 1);
        b[0] = blink1.REPORT2_ID;
        return b;
    })(),
};

function handleSetReport(ev) {
    if (ev.rtype !== ReportType.FEATURE) {
        device.replySetReport(ev.id, 22 /* EINVAL */);
        return;
    }

    const data = ev.data; // id-prefixed, eg [1, cmd, args...]
    const cmd = String.fromCharCode(data[1]);
    state.lastCommand = cmd;

    if (ev.rnum === blink1.REPORT_ID) {
        switch (cmd) {
            case "v":
                state.feature1Reply = blink1.buildVersionResponse(state.version);
                break;
            case "c":
                state.rgb = { r: data[2], g: data[3], b: data[4] };
                state.fadeMillis = ((data[5] << 8) | data[6]) * 10;
                state.ledn = data[7];
                state.feature1Reply = Buffer.from(data);
                break;
            case "n":
                state.rgb = { r: data[2], g: data[3], b: data[4] };
                state.feature1Reply = Buffer.from(data);
                break;
            default:
                state.feature1Reply = Buffer.from(data);
                break;
        }
        device.replySetReport(ev.id, 0);
    } else if (ev.rnum === blink1.REPORT2_ID) {
        if (cmd === "U" && state.version >= 300) {
            const reply = Buffer.alloc(blink1.REPORT2_LENGTH + 1);
            reply[0] = blink1.REPORT2_ID;
            reply[1] = "U".charCodeAt(0);
            reply.write("CAFED00D", 2, "ascii");
            state.feature2Reply = reply;
        } else {
            const reply = Buffer.alloc(blink1.REPORT2_LENGTH + 1);
            ev.data.copy(reply, 0, 0, Math.min(ev.data.length, reply.length));
            reply[0] = blink1.REPORT2_ID;
            state.feature2Reply = reply;
        }
        device.replySetReport(ev.id, 0);
    } else {
        device.replySetReport(ev.id, 22 /* EINVAL */);
    }
}

function handleGetReport(ev) {
    if (ev.rtype !== ReportType.FEATURE) {
        device.replyGetReport(ev.id, 22 /* EINVAL */, Buffer.alloc(0));
        return;
    }

    if (ev.rnum === blink1.REPORT_ID) {
        device.replyGetReport(ev.id, 0, state.feature1Reply);
    } else if (ev.rnum === blink1.REPORT2_ID) {
        device.replyGetReport(ev.id, 0, state.feature2Reply);
    } else {
        // Exercises node-hid's "could not get feature report" error path
        device.replyGetReport(ev.id, 22 /* EINVAL */, Buffer.alloc(0));
    }
}

/** The devtmpfs node exists as soon as the kernel binds hidraw; no udev needed here */
async function findHidrawPath(uniq, timeoutMs = 5000) {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
        for (const entry of fs.readdirSync("/sys/class/hidraw")) {
            try {
                const uevent = fs.readFileSync(path.join("/sys/class/hidraw", entry, "device", "uevent"), "utf8");
                const match = uevent.match(/^HID_UNIQ=(.*)$/m);
                if (match && match[1] === uniq) return `/dev/${entry}`;
            } catch (e) {
                // raced with device removal; keep scanning
            }
        }
        if (Date.now() > deadline) throw new Error(`hidraw node for uniq ${uniq} did not appear`);
        await new Promise((resolve) => setTimeout(resolve, 10));
    }
}

const commands = {
    async create(args) {
        device = new UhidDevice({
            name: args.name,
            uniq: args.serial,
            vendorId: blink1.VENDOR_ID,
            productId: blink1.PRODUCT_ID,
            version: 0x0002,
            reportDescriptor: blink1.REPORT_DESCRIPTOR,
        });
        state.version = args.version;

        device.on("setReport", handleSetReport);
        device.on("getReport", handleGetReport);
        device.on("output", (data, rtype) => {
            process.send({ event: "output", data: [...data], rtype });
        });
        device.on("error", (err) => {
            process.send({ event: "error", message: err.message });
        });

        await device.create();
        return { hidrawPath: await findHidrawPath(args.serial) };
    },

    async sendInput(args) {
        device.sendInput(Buffer.from(args.data));
        return {};
    },

    async getState() {
        return {
            version: state.version,
            rgb: state.rgb,
            fadeMillis: state.fadeMillis,
            ledn: state.ledn,
            lastCommand: state.lastCommand,
            feature1Reply: [...state.feature1Reply],
            feature2Reply: [...state.feature2Reply],
        };
    },

    async destroy() {
        device.destroy();
        return {};
    },

    async close() {
        device.close();
        return {};
    },
};

process.on("message", (msg) => {
    const impl = commands[msg.cmd];
    Promise.resolve()
        .then(() => impl(msg.args))
        .then(
            (result) => process.send({ id: msg.id, ok: true, result }),
            (err) => process.send({ id: msg.id, ok: false, error: err.message })
        );
});

process.on("disconnect", () => {
    if (device) device.close();
    process.exit(0);
});
