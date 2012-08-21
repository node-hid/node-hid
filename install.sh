#!/bin/sh

set -e

cd src
node-waf clean configure build install
