#!/bin/bash

# fpp-midi uninstall script
echo "Running fpp-midi uninstall Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make clean


# No restartFlag: the Plugin Manager unloads the plugin through fppd before it
# removes these files, so the uninstall has already taken effect by the time
# this runs.
