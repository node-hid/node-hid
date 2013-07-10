#!/bin/sh

if [ -d hidapi ]
then
    cd hidapi
    git pull
else
    git clone http://github.com/signal11/hidapi
fi
