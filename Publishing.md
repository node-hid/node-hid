How to publish node-hid
========================


1. First, make sure version is bumped to new version. (if code change)

2. Push changes to Github, and wait for the prebuild workflow to complete

3. Run: 
```
npm run clean # clean out directory
```

4. Download the `all-prebuilds` artifact, and extract as a `prebuilds` folder in the root of the repository

5. Run: 
```
npm publish  # update npmjs, be sure to have Authy app for OTP code
```

-----

If desired, manual testing can be down on each of MacOS, Windows, Linux, do:
```
git clone https://github.com/node-hid/node-hid.git
cd node-hid
npm run distclean                 # if not a fresh checkout
npm run prepublishOnly            # get the hidapi submodule
npm install --build-from-source   # rebuilds the C code
npm run showdevices               # simple test
node ./src/test-blink1.js         # simple test
```
