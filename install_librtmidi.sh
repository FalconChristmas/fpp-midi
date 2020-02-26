#!/bin/bash
cd /tmp
rm -rf librtmidi-*
apt-get -q -y update
apt-get -q -y --reinstall install librtmidi4
apt-get clean
apt-get download librtmidi-dev
rm -rf deb
mkdir deb
dpkg-deb -R ./librtmidi-dev*.deb  deb
sed -i -e "s/Version\(.*\)+\(.*\)/Version\1~fpp/g" deb/DEBIAN/control
sed -i -e "s/Depends: \(.*\)/Depends: librtmidi4/g" deb/DEBIAN/control
dpkg-deb -b deb ./librtmidi-dev.deb
apt-get -y --reinstall install ./librtmidi-dev.deb
apt-mark hold librtmidi-dev
apt-get clean
rm -rf deb
