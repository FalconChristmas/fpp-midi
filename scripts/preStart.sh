#!/bin/sh

echo "Running fpp-osc PreStart Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make
