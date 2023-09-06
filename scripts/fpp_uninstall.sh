#!/bin/bash

# fpp-midi uninstall script
echo "Running fpp-midi uninstall Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make clean

