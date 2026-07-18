/**
 * Decides how the device test suites run:
 *   mode 'fake'  - Linux with writable /dev/uhid: fake blink(1) via uhid
 *   mode 'real'  - NODE_HID_TEST_REAL_BLINK1=1: a physically attached blink(1)
 *   mode 'skip'  - neither available; suites skip with an explanation, except in
 *                  CI where a skip would silently mask lost coverage, so it throws
 */
const fs = require("fs");

// hidapi's linux hidraw backend needs a UTF-8 LC_CTYPE to convert non-ascii
// device strings; the default "C" locale turns them into empty strings. The
// addon honours this env var as a TEST-ONLY opt-in (see ApplicationContext::get).
// It must be set before the native binding is first used, which is guaranteed
// here because the binding is lazy-loaded on first API call
if (process.platform === "linux" && !process.env.NODE_HID_TEST_SET_LOCALE) {
    process.env.NODE_HID_TEST_SET_LOCALE = "C.UTF-8";
}

function resolveMode() {
    if (process.env.NODE_HID_TEST_REAL_BLINK1 === "1") {
        return { mode: "real", skipReason: false };
    }

    if (process.platform !== "linux") {
        return skip("fake device requires Linux /dev/uhid; set NODE_HID_TEST_REAL_BLINK1=1 with a blink(1) attached to test on this platform");
    }

    if (!fs.existsSync("/dev/uhid")) {
        return skip("/dev/uhid not available (try: sudo modprobe uhid)");
    }

    try {
        fs.accessSync("/dev/uhid", fs.constants.R_OK | fs.constants.W_OK);
    } catch (e) {
        return skip(
            "/dev/uhid is not accessible. Install the test udev rules (see test/60-node-hid-test.rules), " +
                'or run the suite as root: sudo env "PATH=$PATH" npm run test:device'
        );
    }

    return { mode: "fake", skipReason: false };
}

function skip(reason) {
    if (process.env.CI) {
        throw new Error(`device tests unavailable in CI (refusing to silently skip): ${reason}`);
    }
    return { mode: "skip", skipReason: reason };
}

const resolved = resolveMode();

module.exports = {
    mode: resolved.mode,
    /** Truthy skip reason for the fake-OR-real core suite */
    skipReason: resolved.skipReason,
    /** Truthy skip reason for suites that only work against the fake device */
    fakeOnlySkipReason:
        resolved.mode === "real" //
            ? "running against real hardware; fake-only suite skipped"
            : resolved.skipReason,
};
