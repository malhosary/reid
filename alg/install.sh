#!/bin/bash
###
 # @Description: Algorithm install script.
 # @version: 1.0
 # @Author: Ricardo Lu<shenglu1202@163.com>
 # @Date: 2021-11-17 17:16:17
 # @LastEditors: Ricardo Lu
 # @LastEditTime: 2021-11-17 17:21:19
### 

set -x

CURUSER=`whoami`
SUDO=""

[ -n "$INSTALL_DIR"    ] || INSTALL_DIR="/opt/thundersoft"
[ -n "$INSTALL_ALL"    ] || INSTALL_ALL="YES"
[ "$CURUSER" == "root" ] || SUDO="sudo"

#--------------------------------------------------------------------------------------------------
# begin install dependencies  
#--------------------------------------------------------------------------------------------------
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
#--------------------------------------------------------------------------------------------------
# end install dependencies
#--------------------------------------------------------------------------------------------------

OK=`env | grep LD_LIBRARY_PATH | grep "$INSTALL_DIR/algs/lib"`
if [ "$OK" == "" ]; then
  export LD_LIBRARY_PATH=$INSTALL_DIR/algs/lib:$LD_LIBRARY_PATH
fi

if [ ! -d "$INSTALL_DIR/algs/include" ]; then
  $SUDO mkdir -p $INSTALL_DIR/algs/include
fi

if [ ! -d "$INSTALL_DIR/algs/lib" ]; then
  $SUDO mkdir -p $INSTALL_DIR/algs/lib
fi

if [ ! -d "$INSTALL_DIR/algs/model" ]; then
  $SUDO mkdir -p $INSTALL_DIR/algs/model
fi

if [ ! -d "$INSTALL_DIR/configs" ]; then
  $SUDO mkdir -p $INSTALL_DIR/configs
fi

#--------------------------------------------------------------------------------------------------
# begin build sdk 
#--------------------------------------------------------------------------------------------------
if [ "$INSTALL_ALL" == "YES" ]; then
  if [ -d sdk ]; then
    cd sdk
    rm -rf build
    mkdir build
    cd build
    cmake ..
    make -j
    make install
    cd ../..
  fi
fi
#--------------------------------------------------------------------------------------------------
# end build sdk 
#--------------------------------------------------------------------------------------------------

#--------------------------------------------------------------------------------------------------
# begin build alg reid
#--------------------------------------------------------------------------------------------------
if [ -d reid ]; then
  cd reid
  rm -rf build
  mkdir build
  cd build
  cmake ..
  make -j
  $SUDO make install
  cd ../..
fi
#--------------------------------------------------------------------------------------------------
# end build alg reid
#--------------------------------------------------------------------------------------------------