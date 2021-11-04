#!/bin/bash
###
 # @Description: Stream Pipeline install script.
 # @version: 1.0
 # @Author: Ricardo Lu<shenglu1202@163.com>
 # @Date: 2021-10-27 10:39:10
 # @LastEditors: Ricardo Lu
 # @LastEditTime: 2021-10-28 10:38:05
### 

set -x

CURUSER=`whoami`
SUDO=""

[ -n "$INSTALL_DIR"    ] || INSTALL_DIR="/opt/thundersoft"
[ -n "$INSTALL_ALL"    ] || INSTALL_ALL="YES"
[ "$CURUSER" == "root" ] || SUDO="sudo"

# install dependences
INSTALLED=`dpkg -l | grep -E '^ii' | grep libjson-glib-dev`
if [[ -z "$INSTALLED" ]]; then
  $SUDO apt-get install libjson-glib-dev
fi
INSTALLED=`dpkg -l | grep -E '^ii' | grep libgflags-dev`
if [[ -z "$INSTALLED" ]]; then
  $SUDO apt-get install libgflags-dev
fi
INSTALLED=`dpkg -l | grep -E '^ii' | grep uuid-dev`
if [[ -z "$INSTALLED" ]]; then
  $SUDO apt-get install uuid-dev
fi
INSTALLED=`dpkg -l | grep -E '^ii' | grep libmosquitto-dev`
if [[ -z "$INSTALLED" ]]; then
  $SUDO apt-get install libmosquitto-dev
fi
INSTALLED=`dpkg -l | grep -E '^ii' | grep mosquitto`
if [[ -z "$INSTALLED" ]]; then
  $SUDO apt-get install mosquitto
fi

# new install directory
if [ ! -d "$INSTALL_DIR/spls/include" ]; then
  $SUDO mkdir -p $INSTALL_DIR/spls/include
fi
if [ ! -d "$INSTALL_DIR/spls/lib" ]; then
  $SUDO mkdir -p $INSTALL_DIR/spls/lib
fi
if [ ! -d "$INSTALL_DIR/configs" ]; then
  $SUDO mkdir -p $INSTALL_DIR/configs
fi

OK=`env | grep LD_LIBRARY_PATH | grep "$INSTALL_DIR/spls/lib"`
if [ "$OK" == "" ]; then
  export LD_LIBRARY_PATH=$INSTALL_DIR/spls/lib:$LD_LIBRARY_PATH
fi

if [ ! -d "./debs" ]; then
  mkdir -p ./debs
fi

# build
rm -rf build
mkdir build
cd build
cmake ..
make -j18
$SUDO make install
cd ..