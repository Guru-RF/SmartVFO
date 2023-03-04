#!/bin/bash

DIR="/Volumes/RPI-RP2"
if [ -d "$DIR" ]; then
  echo "Installing firmwire to pico in ${DIR}..."
  cd /tmp
  wget https://downloads.circuitpython.org/bin/raspberry_pi_pico/en_US/adafruit-circuitpython-raspberry_pi_pico-en_US-7.3.3.uf2
  cp adafruit-circuitpython-raspberry_pi_pico-en_US-7.3.3.uf2 /Volumes/RPI-RP2
  echo "Sleeping 10 seconds for firmware to install"
  cd -
  sleep 10
fi

DIR="/Volumes/CIRCUITPY"
if [ -d "$DIR" ]; then
  echo "Install software in ${DIR}..."
  cp -r lib /Volumes/CIRCUITPY
  cp *.py /Volumes/CIRCUITPY
  diskutil unmount /Volumes/CIRCUITPY
  echo "done"
fi

DIR="/Volumes/SMARTPWR"
if [ -d "$DIR" ]; then
  echo "Updating software in ${DIR}..."
  cp -r lib /Volumes/SMARTPWR
  cp *.py /Volumes/SMARTPWR
  echo "done"
fi
