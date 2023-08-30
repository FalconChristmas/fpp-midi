#!/bin/sh

echo "Running fpp-midi PreStart Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make
