#!/bin/sh

set -e

git submodule init
git submodule update

cd src
node-waf clean configure build install
