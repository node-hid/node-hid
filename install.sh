#!/bin/sh

set -e

[ -d build ] && node-gyp clean
node-gyp configure build install
