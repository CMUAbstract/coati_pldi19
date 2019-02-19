#!/bin/bash
# This script is for compiling and flashing the coati benchmarks
# arguments:
#   $1: 1/0 for continuously powered or harvested energy
#   $2: bc, ar, cem, rsa, cuckoo or blowfish are the different app choices
#
#Example: ./scripts/run_coati_app.sh 0 bc
# **Update the error message in line 15 with the path to the coati_public
# directory**

set -e
app=$2

dir=$( pwd -P)
echo $dir
if [ $dir != "/home/reviewer/coati_public" ]; then
  echo "ERROR: Scripts must be run from ~/coati_public !"
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

declare -a arr=("bc" "ar" "cem" "rsa" "blowfish" "cuckoo" )

match=0
for prog in "${arr[@]}"
  do
    if [ "$app" ==  "$prog" ]; then
      match=1
    fi
done

if [ $match -eq 0 ]; then
  echo "Error- app does not exist! Options are: bc, ar, rsa, cem, cuckoo, blowfish"
  exit 1
fi
echo "Start depclean"


make -s apps/coati/bld/gcc/depclean LIBCOATIGCC_BUFFER_ALL=1 &> error.out

echo "Building deps"

make -s apps/coati/bld/gcc/dep LIBCOATIGCC_BUFFER_ALL=1 \
LIBCOATIGCC_CHECK_ALL_TX=$is_cuckoo LIBCAPYBARA_CONT_POWER=$1 &> error.out

# Build the directories me need
mkdir -p ../outputs/results
mkdir -p ../outputs/results/coati/
if [ $1 -eq 0 ]; then
  mkdir -p ../outputs/results/coati/harv/
  dir_start=../outputs/results/coati/harv
else
  mkdir -p ../outputs/results/coati/cont/
  dir_start=../outputs/results/coati/cont
fi

echo "Start coati all"
# Compile the app code
make -s apps/coati/bld/gcc/dep LIBCOATIGCC_BUFFER_ALL=0 \
LIBCAPYBARA_CONT_POWER=$1 &> error.out
# now program, change to match USB port of FET
mspdebug -v 2400 -d /dev/ttyACM0 tilib "prog apps/coati/bld/gcc/coati_test.out"
# Change to match USB port of uart converter, you'll need to manually kill
# the screen session when the app is finished running
screen -L /dev/ttyUSB0 115200
# This is going to overwrite old files
filename="${dir_start}/${app}_run${i}.txt"
mv screenlog.0 $filename
echo "Reporting differences:"
set +e
diff $filename expected_outputs/${app}_results.txt
echo "Diff complete"
set -e
# Clear out the old executables
rm apps/coati/bld/gcc/*.out
rm apps/coati/bld/gcc/main_*

