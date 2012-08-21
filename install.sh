#!/bin/sh

set -e

cd src
[ -d build ] && node-waf clean
node-waf configure build install
