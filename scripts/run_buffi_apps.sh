#!/bin/bash
# This script is for testing the buffi benchmarks
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

# We have to run cuckoo with CHECK_ALL_TX enabled
declare -a arr=("bc" "ar" "cem" "rsa" "blowfish")

echo "Start depclean"

make -s apps/buffi/bld/gcc/depclean LIBCOATIGCC_BUFFER_ALL=1 &> /tmp/artifact_results/error.out

echo "Building deps"

make -s apps/buffi/bld/gcc/dep LIBCOATIGCC_BUFFER_ALL=1 \
LIBCAPYBARA_CONT_POWER=$1 &> /tmp/artifact_results/error.out
# Build the directories me need
mkdir -p /tmp/artifact_results
mkdir -p /tmp/artifact_results/buffi/
if [ $1 -eq 0 ]; then
  mkdir -p /tmp/artifact_results/buffi/harv/
  dir_start=/tmp/artifact_results/buffi/harv
else
  mkdir -p /tmp/artifact_results/buffi/cont/
  dir_start=/tmp/artifact_results/buffi/cont
fi

echo "Testing apps"
for app in "${arr[@]}"
  do
  echo "Start buffi all"
  for i in {1..1}
    do
    # Compile the app code
    make -s VERBOSE=1 apps/buffi/bld/gcc/all TEST=$app \
    LIBCOATIGCC_BUFFER_ALL=1 LIBCAPYBARA_CONT_POWER=$1 &> /tmp/artifact_results/error.out
    echo "Starting saleae with args: $app $power"
    # Turn on the saleae in background
    python scripts/support/coati-capture.py buffi $app $power &
    # Get process id
    SALEAE_ID=$!
    # Start screen in the background
    screen -L -dmS cur /dev/ttyUSB0 115200
    # Send a signal to the arduion to trigger the relay
    python scripts/support/arduino_comm.py /dev/ttyACM2 E
    python scripts/support/arduino_comm.py /dev/ttyACM2 P
    # now program
    mspdebug -v 2400 -d /dev/ttyACM0 tilib "prog apps/buffi/bld/gcc/coati_test.out"
    # Turn off the relay if we're running on harvested energy
    if [ $1 -eq 0 ]; then
      python scripts/support/arduino_comm.py /dev/ttyACM2 H
    fi
    # Start the interrupts
    python scripts/support/arduino_comm.py /dev/ttyACM2 S
    # When this terminates, end screen and move the output
    wait $SALEAE_ID
    # When this terminates, end screen and move the output
    echo "Killing screen"
    screen -X -S cur quit
    # This is going to overwrite old files
    filename="${dir_start}/${app}_run${i}.txt"
    mv screenlog.0 $filename
    echo "Reporting differences:"
    set +e
    diff $filename expected_outputs/${app}_results.txt
    if [ $1 -eq 0 ]; then
      python scripts/support/harv_pwrd_runtimes.py "buffi" "${app}"
    else
      python scripts/support/cont_pwrd_runtimes.py "buffi" "${app}"
    fi
    ./scripts/support/all_event_scrape "buffi" "${app}" $1
    echo "Diff complete"
    set -e
    echo "Done buffi $app run $i"
  done
    # Clear out the old executables
    rm apps/buffi/bld/gcc/*.out
    rm apps/buffi/bld/gcc/main_*
done

echo "Start depclean"

make -s apps/buffi/bld/gcc/depclean LIBCOATIGCC_BUFFER_ALL=1

echo "Rebuilding deps"

make -s apps/buffi/bld/gcc/dep LIBCOATIGCC_BUFFER_ALL=1 \
LIBCOATIGCC_CHECK_ALL_TX=1 LIBCAPYBARA_CONT_POWER=$1

echo "Start buffi cuckoo"
for i in {1..1}
  do
  # Compile the app code
  make -s VERBOSE=1 apps/buffi/bld/gcc/all TEST=cuckoo \
  LIBCOATIGCC_CHECK_ALL_TX=1 LIBCOATIGCC_BUFFER_ALL=1 \
  LIBCAPYBARA_CONT_POWER=$1
  echo "Starting saleae with args: cuckoo $power"
  # Turn on the saleae in background
  python scripts/support/coati-capture.py buffi cuckoo $power &
  # Get process id
  SALEAE_ID=$!
  # Start screen in the background
  screen -L -dmS cur /dev/ttyUSB0 115200
  # Send a signal to the arduion to trigger the relay
  python scripts/support/arduino_comm.py /dev/ttyACM2 E
  python scripts/support/arduino_comm.py /dev/ttyACM2 P
  # now program
  mspdebug -v 2400 -d /dev/ttyACM0 tilib "prog apps/buffi/bld/gcc/coati_test.out"
  # Turn off the relay if we're running on harvested energy
  if [ $1 -eq 0 ]; then
    python scripts/support/arduino_comm.py /dev/ttyACM2 H
  fi
  # Bring the python script back to the foreground
    # When this terminates, end screen and move the output
    wait $SALEAE_ID
    # When this terminates, end screen and move the output
    echo "Killing screen"
    screen -X -S cur quit
    # This is going to overwrite old files
    filename="/tmp/artifact_results/buffi/cuckoo_run${i}.txt"
    mv screenlog.0 $filename
    set +e
    echo "Reporting differences:"
    diff $filename expected_outputs/cuckoo_results.txt
    echo "Diff complete"
    if [ $1 -eq 0 ]; then
      python scripts/support/harv_pwrd_runtimes.py "buffi" "cuckoo"
    else
      python scripts/support/cont_pwrd_runtimes.py "buffi" "cuckoo"
    fi
    ./scripts/support/all_event_scrape.sh "buffi" "cuckoo" $1
    set -e
  echo "Done buffi cuckoo run $i"
done
# Build the directories me need

# Process saleae, report runtime
# Process events and return percent captured
