#!/bin/bash

# fpp-osc uninstall script
echo "Running fpp-osc uninstall Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make clean

