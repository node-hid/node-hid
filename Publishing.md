How to publish node-hid
========================


1. First, make sure version is bumped to new version. (if code change)

2. Then, on each of MacOS, Windows, Linux, do:
```
git clone https://github.com/node-hid/node-hid.git
cd node-hid
npm run distclean                 # if not a fresh checkout
npm run prepublishOnly            # get the hidapi submodule
npm install --build-from-source   # rebuilds the C code
npm run showdevices               # simple test
node ./src/test-blink1.js         # simple test
npm run prebuild                  # build all the versions
npm run prebuild-upload <GH_TOKEN> # upload all the versions using github token
```

3. And then on master dev box:
```
npm run clean # clean out directory
npm publish  # update npmjs, be sure to have Authy app for OTP code
```
