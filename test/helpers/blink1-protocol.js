/**
 * The subset of the blink(1) USB protocol used by the tests, shared between the
 * fake device (uhid-host.js) and the test suites. Matches src/test-blink1.js.
 */

const VENDOR_ID = 0x27b8;
const PRODUCT_ID = 0x01ed;

const REPORT_ID = 1;
const REPORT2_ID = 2;
const REPORT_LENGTH = 8; // payload; 9 bytes on the wire including the report id
const REPORT2_LENGTH = 60; // payload; 61 bytes on the wire including the report id

/**
 * Feature reports use ids 1 and 2, so the (fake-only) input report must be
 * numbered too - mixing numbered and unnumbered reports is invalid HID. A real
 * blink(1) has no input report; only the fake-only test suites rely on it.
 */
const REPORT_DESCRIPTOR = Buffer.from([
    0x06, 0x00, 0xff, // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01, //       Usage (0x01)
    0xa1, 0x01, //       Collection (Application)
    0x15, 0x00, //         Logical Minimum (0)
    0x26, 0xff, 0x00, //   Logical Maximum (255)
    0x75, 0x08, //         Report Size (8 bits)
    0x85, REPORT_ID, //    Report ID (1)
    0x95, REPORT_LENGTH, //  Report Count (8)
    0x09, 0x01, //         Usage (0x01)
    0xb1, 0x02, //         Feature (Data,Var,Abs)
    0x09, 0x01, //         Usage (0x01)
    0x81, 0x02, //         Input (Data,Var,Abs) - fake-only
    0x85, REPORT2_ID, //   Report ID (2)
    0x95, REPORT2_LENGTH, // Report Count (60)
    0x09, 0x01, //         Usage (0x01)
    0xb1, 0x02, //         Feature (Data,Var,Abs)
    0xc0, //             End Collection
]);

/** Build the 9-byte feature report for a report-1 command, eg command('v') */
function command(cmd, ...args) {
    const report = new Array(REPORT_LENGTH + 1).fill(0);
    report[0] = REPORT_ID;
    report[1] = cmd.charCodeAt(0);
    for (let i = 0; i < args.length; i++) report[2 + i] = args[i];
    return report;
}

/** Build the 61-byte feature report for a report-2 command */
function command2(cmd, ...args) {
    const report = new Array(REPORT2_LENGTH + 1).fill(0);
    report[0] = REPORT2_ID;
    report[1] = cmd.charCodeAt(0);
    for (let i = 0; i < args.length; i++) report[2 + i] = args[i];
    return report;
}

/** The 'c' fade-to-color command, as blink1_fadeToColor in test-blink1.js */
function fadeToColor(fadeMillis, r, g, b, ledn = 0) {
    const dms = Math.floor(fadeMillis / 10);
    return command("c", r, g, b, dms >> 8, dms % 0xff, ledn);
}

/** Parse the firmware version out of a 'v' response, as test-blink1.js does */
function parseVersionResponse(response) {
    return (
        parseInt(String.fromCharCode(response[3])) * 100 + //
        parseInt(String.fromCharCode(response[4]))
    );
}

/** Build the 'v' response payload for a given version (the fake side) */
function buildVersionResponse(version) {
    const reply = Buffer.alloc(REPORT_LENGTH + 1);
    reply[0] = REPORT_ID;
    reply[1] = "v".charCodeAt(0);
    reply[3] = String(Math.floor(version / 100)).charCodeAt(0);
    reply[4] = String(version % 100).charCodeAt(0);
    return reply;
}

module.exports = {
    VENDOR_ID,
    PRODUCT_ID,
    REPORT_ID,
    REPORT2_ID,
    REPORT_LENGTH,
    REPORT2_LENGTH,
    REPORT_DESCRIPTOR,
    command,
    command2,
    fadeToColor,
    parseVersionResponse,
    buildVersionResponse,
};
