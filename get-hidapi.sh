#!/bin/sh

if [ -d hidapi ]
then
    cd hidapi
    git pull
else
    git clone https://github.com/signal11/hidapi.git
fi
