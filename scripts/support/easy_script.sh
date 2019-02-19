#!/bin/bash

# script for running unit tests
# args:
# system(coati,buffi,alpaca) compiler(gcc,alpaca) test(bc,ar,etc) power(1/0)
set -e
make -s apps/$1/bld/$2/depclean TEST=$3
make -s apps/$1/bld/$2/dep TEST=$3 LIBCAPYBARA_CONT_POWER=$4
make -s apps/$1/bld/$2/all TEST=$3 VERBOSE=1 LIBCAPYBARA_CONT_POWER=$4

screen -L -dmS cur /dev/ttyUSB0 115200

if [ $4 -eq 0 ]; then
  power="harv"
else
  power="cont"
fi

# Turn on the saleae in background
python scripts/support/coati-capture.py $1 $3 $power &

python scripts/support/arduino_comm.py /dev/ttyACM2 E
python scripts/support/arduino_comm.py /dev/ttyACM2 P
mspdebug -v 2400 tilib "prog apps/$1/bld/$2/coati_test.out"
if [ $4 -eq 0 ]; then
  python scripts/support/arduino_comm.py /dev/ttyACM2 H
fi

python scripts/support/arduino_comm.py /dev/ttyACM2 S

screen -r cur

rm -f apps/$1/bld/$2/*.out
rm -f apps/$1/bld/$2/*.S
rm -f apps/$1/bld/$2/*.bc
rm -f apps/$1/bld/$2/*.d
