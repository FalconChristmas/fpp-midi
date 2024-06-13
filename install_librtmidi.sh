#!/bin/bash
cd /tmp
rm -rf librtmidi-*
apt-get -q -y update

DEBVERF=$(cat /etc/debian_version)
DEBVER=${DEBVERF%.*}
if (( DEBVER < 11 )); then
apt-get -q -y --reinstall install librtmidi4
elif (( DEBVER < 12 )); then
apt-get -q -y --reinstall install librtmidi5
else
apt-get -q -y --reinstall install librtmidi6
fi
apt-get clean
apt-get download librtmidi-dev
rm -rf deb
mkdir deb
dpkg-deb -R ./librtmidi-dev*.deb  deb
sed -i -e "s/Version\(.*\)+\(.*\)/Version\1~fpp/g" deb/DEBIAN/control

if (( DEBVER < 11 )); then
sed -i -e "s/Depends: \(.*\)/Depends: librtmidi4/g" deb/DEBIAN/control
elif (( DEBVER < 11 )); then
sed -i -e "s/Depends: \(.*\)/Depends: librtmidi5/g" deb/DEBIAN/control
else 
sed -i -e "s/Depends: \(.*\)/Depends: librtmidi6/g" deb/DEBIAN/control
fi
dpkg-deb -b deb ./librtmidi-dev.deb
apt-get -y --reinstall --allow-change-held-packages install ./librtmidi-dev.deb
apt-mark hold librtmidi-dev
apt-get clean
rm -rf deb
