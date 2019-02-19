#!/bin/bash
# This script is for testing the alpaca benchmarks
# arguments:
#   $1: 1/0 for continuously powered or harvested energy

set -e

dir=$( pwd -P)
echo $dir
if [ $dir != "~/coati_test" ]; then
  echo "ERROR: Scripts must be run from ~/coati_test !"
  exit 1
fi

if [ $# -eq 0 ]
  then
    echo "No arguments supplied, enter 1/0 for continuously powered or harvested energy"
    exit 1
fi

if [ $1 -eq 0 ]; then
  power="harv"
else
  power="cont"
fi

declare -a arr=("bc" "ar" "cem" "rsa" "cuckoo" "blowfish")

echo "Start depclean"

make -s apps/alpaca/bld/alpaca/depclean &> /tmp/artifact_results/error.out

echo "Building deps"

make -s apps/alpaca/bld/alpaca/dep \
LIBCAPYBARA_CONT_POWER=$1 &> /tmp/artifact_results/error.out
# Build the directories me need
mkdir -p /tmp/artifact_results
mkdir -p /tmp/artifact_results/alpaca/
if [ $1 -eq 0 ]; then
  mkdir -p /tmp/artifact_results/alpaca/harv/
  dir_start=/tmp/artifact_results/alpaca/harv
else
  mkdir -p /tmp/artifact_results/alpaca/cont
  dir_start=/tmp/artifact_results/alpaca/cont/
fi

echo "Testing apps"
for app in "${arr[@]}"
  do
  echo "Start alpaca all"
  for i in {1..1}
    do
    # Compile the app code
    make -s VERBOSE=1 apps/alpaca/bld/alpaca/all TEST=$app \
    LIBCAPYBARA_CONT_POWER=$1 &> /tmp/artifact_results/error.out
    echo "Starting saleae with args: $app $power"
    # Turn on the saleae in background
    python scripts/support/coati-capture.py alpaca $app $power &
    # Get process id
    SALEAE_ID=$!
    # Start screen in the background
    screen -L -dmS cur /dev/ttyUSB0 115200
    # Send a signal to the arduion to trigger the relay
    python scripts/support/arduino_comm.py /dev/ttyACM2 E
    python scripts/support/arduino_comm.py /dev/ttyACM2 P
    # now program
    mspdebug -v 2400 -d /dev/ttyACM0 tilib "prog apps/alpaca/bld/alpaca/coati_test.out"
    # Turn off the relay if we're running on harvested energy
    if [ $1 -eq 0 ]; then
      python scripts/support/arduino_comm.py /dev/ttyACM2 H
    fi
    # Start the interrupts
    python scripts/support/arduino_comm.py /dev/ttyACM2 S
    # Use wait command based on process id
    wait $SALEAE_ID
    # When this terminates, end screen and move the output
    echo "Killing screen"
    # When this terminates, end screen and move the output
    screen -X -S cur quit
    filename="${dir_start}/${app}_run${i}.txt"
    mv screenlog.0 $filename
    set +e
    echo "Reporting differences:"
    diff $filename expected_outputs/${app}_results.txt > \
      "${dir_start}/${app}_run${i}_diffs.txt"
    echo "Diff complete"
    if [ $1 -eq 0 ]; then
      python scripts/support/harv_pwrd_runtimes.py "alpaca" "${app}"
    else
      python scripts/support/cont_pwrd_runtimes.py "alpaca" "${app}"
    fi
    set -e
    echo "Done alpaca $app run $i"
  done
    # Clear out the old executables
    rm apps/alpaca/bld/alpaca/main_*
    rm apps/alpaca/bld/alpaca/coati_test*
done

# process differences
# Process saleae, report runtime
# Process events and return percent captured
