How to publish node-hid
========================

### newer way (in short) ###

First, make sure version is bumped to new version. (if code change)

Then, on each of MacOSX, Windows, Linux, do:
```
git clone https://github.com/node-hid/node-hid.git
cd node-hid     
npm run prepublish                # get the hidapi submodule
npm install --build-from-source   # rebuilds the C code
node ./src/show-devices.js        # simple tests
npm run prebuild                  # build all the versions
npm run prebuild-upload <gh-token> # upload all the versions using github token
```
And then on master dev box:
```
npm run clean # clean out directory
npm publish  # update npmjs, be sure to have Authy app for OTP code
```

remember for Windows
```
$env:PYTHON = "$env:USERPROFILE\.windows-build-tools\python27\python.exe"
```


#### notes ####
- As of Node v10 on Windows, may need to do:
-- `$env:Path += ";C:\Program Files\Git\mingw64\libexec\git-core
-- and install git with "make unix tools available to windows command prompt"

#### old way ###

1. Merge all changes and new features into master
2. Bump up npm version in `package.json`
3. Update the `README.md` to reference this current version (if needed)
4. Commit then generate new tags based on package.json version number
   with e.g. `git tag v0.5.4 -a` and include the change log in the tag's annotation.
5. Push tags to Github with `git push --tags`
6. Watch Travis and Appveyor CI builds
7. Publish to npm after builds finish. Builds can take half an hour and occasionally fail for seemingly no reason. Restart any failures in the travis or appeveyor ui. While you wait, remove the content of the Github release message so the tag's text shows. When the entire matrix succeeds and all binaries exist run `npm publish`.


## Config Travis, AppVeyor and Github to generate all of the binaries.

Before we are able to run everything stated above some steps need to be taken. Specifically for being able to publish the pre compiled binaries to Github. The correct keys need to be setup in the `travis.yml` and `appveyor.yml` files. For Travis, this needs to be done by the admin of the Github repo. For AppVeyor, this will need to be done by the owner of the AppVeyor account.

### Setting up secure keys in Travis.

Setting up the keys in Travis is easy if you have Ruby and Rubygems installed and working then run:

`gem install travis`

After the Travis gem is installed run the following command for each of the required keys:

`travis encrypt SOMEVAR=secretvalue`

And substitute the values in the `.travis.yml` file for the new ones. Detailed instructions can
be found here: http://docs.travis-ci.com/user/environment-variables/#Secure-Variables

### Setting up secure keys in AppVeyor

It is even easier than Travis to configure AppVeyor. You do not need to install anything, just go to your account and click on `encrypt tool`. Enter the values in the input field and click "Encrypt". In the same way as we do for Travis, we then need to substitute the newly generated values for the old ones.

Detailed instructions can be found here: http://www.appveyor.com/docs/build-configuration#secure-variables

-----

## Hand publish method using `node-pre-gyp-github`
* Install tools globally to make things easier:
```
npm install -g node-pre-gyp
npm install -g node-pre-gyp-github
npm install -g rimraf
```
* Checkout clean node-hid:
```
rm -rf node-hid
git clone https://github.com/node-hid/node-hid.git
cd node-hid
npm run prepublish
```

* Clean checkout before each build:
```
npm run clean
npm install
```

* Do simple sanity tests
```
node ./src/show-devices.js
node ./src/test-blink1.js
```

* Test out node-pre-gyp packaging and publish draft:
```
node-pre-gyp configure
node-pre-gyp build
node-pre-gyp package
node-pre-gyp-github publish
```

* Do the above for each version of node:
  * v6.9.1 LTS
  * v4.6.2 LTS
  * v0.12.17


### Windows notes:
* Using nvm-windows, install each version of node needed (32-bit & 64-bit):
```
nvm install v0.12.7 32
nvm install v0.12.7 64
nvm install v4.6.2 32
nvm install v4.6.2 64
nvm install v6.9.1 32
nvm install v6.9.1 64
```

* Then, before each build:
```
nvm use v0.12.7 32
node install -g node-pre-gyp
node install -g node-pre-gyp-github
node install -g rimraf
npm run clean
npm install

```



## Old publish method

(cribbed from node-serialport)

## Setup for Linux, Windows and OSX

1. Merge all changes and new features into master.
2. Fill out changelog.md.
3. Bump up npm version *AND* binary host version in `package.json`, commit and push.
4. Update the README.md to reference this current version and to previous major version docs.
5. Generate new tags based on package.json version number with `git tag 4.0.0 -a` and include the change log in the tag's annotation.
6. Push tags to Github with `git push --tags`
7. Switch to node v0.12 and npm 2
8. `rm -rf node_modules build && npm install`
9. Publish to npm after builds finish. Builds can take half an hour and occasionally fail for seemingly no reason. Restart any failures in the travis or appeveyor ui. While you wait, remove the content of the Github release message so the tag's text shows. When the entire matrix succeeds and all binaries exist run `npm publish`.

Differences for beta release
* Tag like: `git tag 4.0.1-beta1 -a` and include the change log in the tag's annotation.
* Publish with `npm publish --tag beta`


## Binary Hand-publish Notes
* On Mac/Linux, have a ~/.node_pre_gyprc with Amazon AWS S3 accessKeyId & secretAccessKey
* On Windows, put that file in /c/Users/{username} when using MinGW

## Hand-publish steps
1. Pick node version (e.g. "nvm use v4" or "nodist use v4")
2. Update node-hid from master: `git pull`
3. Build: `npm install`
4. Package using node-pre-gyp: `npm run gyppackage`
5. Publish to AWS S3 using node-pre-gyp: `npm run gyppublish`
6. Clean up build products: `npm run clean`
