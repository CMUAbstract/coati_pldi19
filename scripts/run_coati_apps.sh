#!/bin/bash
# This script is for testing the coati benchmarks
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
  echo "Running harv!"
  power="harv"
else
  power="cont"
fi

declare -a arr=("bc" "ar" "cem" "rsa" "cuckoo" "blowfish")

echo "Start depclean"

make -s apps/coati/bld/gcc/depclean LIBCOATIGCC_BUFFER_ALL=0 &> /tmp/artifact_results/error.out

echo "Building deps"

make -s apps/coati/bld/gcc/dep LIBCOATIGCC_BUFFER_ALL=0 \
LIBCAPYBARA_CONT_POWER=$1 &> /tmp/artifact_results/error.out
# Build the directories me need
mkdir -p /tmp/artifact_results
mkdir -p /tmp/artifact_results/coati/

if [ $1 -eq 0 ]; then
  mkdir -p /tmp/artifact_results/coati/harv/
  dir_start=/tmp/artifact_results/coati/harv
else
  mkdir -p /tmp/artifact_results/coati/cont/
  dir_start=/tmp/artifact_results/coati/cont
fi

echo "Testing apps"
for app in "${arr[@]}"
  do
  echo "Start coati all"
  for i in {1..1}
    do
    # Compile the app code
    make -s VERBOSE=1 apps/coati/bld/gcc/all TEST=$app \
    LIBCOATIGCC_BUFFER_ALL=0 LIBCAPYBARA_CONT_POWER=$1 &> /tmp/artifact_results/error.out
    echo "Starting saleae with args: $app $power"
    # Turn on the saleae in background
    python scripts/support/coati-capture.py coati $app $power &
    # Get process id
    SALEAE_ID=$!
    # Start screen in the background
    screen -L -dmS cur /dev/ttyUSB0 115200
    # Send a signal to the arduion to trigger the relay
    python scripts/support/arduino_comm.py /dev/ttyACM2 E
    python scripts/support/arduino_comm.py /dev/ttyACM2 P

    # now program
    mspdebug -v 2400 -d /dev/ttyACM0 tilib "prog apps/coati/bld/gcc/coati_test.out"
    # Turn off the relay if we're running on harvested energy
    if [ $1 -eq 0 ]; then
      python scripts/support/arduino_comm.py /dev/ttyACM2 H
    fi
    # Start the interrupts
    python scripts/support/arduino_comm.py /dev/ttyACM2 S
    # Bring the python script back to the foreground
    # Use wait command based on process id
    wait $SALEAE_ID
    # When this terminates, end screen and move the output
    echo "Killing screen"
    screen -X -S cur quit
    # This is going to overwrite old files
    filename="${dir_start}/${app}_run${i}.txt"
    mv screenlog.0 $filename
    set +e
    echo "Reporting differences:"
    diff $filename expected_outputs/${app}_results.txt > \
      "${dir_start}/${app}_run${i}_diffs.txt"
    if [ $1 -eq 0 ]; then
      python scripts/support/harv_pwrd_runtimes.py "coati" "${app}"
    else
      python scripts/support/cont_pwrd_runtimes.py "coati" "${app}"
    fi
    ./scripts/support/all_event_scrape.sh "coati" "${app}" $1
    set -e
    echo "Done coati $app run $i"
  done
    # Clear out the old executables
    echo "removing old builds"
    rm apps/coati/bld/gcc/*.out
    rm apps/coati/bld/gcc/main_*
done

# Process saleae, report runtime
# Process events and return percent captured
