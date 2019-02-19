Overview --> This file has been modified from the original artifact to remove
all server addresses and log in credentials. It is not recommended to run the
scripts included in the artifact outside of the server setup for which it was
originally intended. It will write files to directories outside of coati_test.
========

This document explains the artifacts for PLDI 2019 paper submission #9:
"Transactional Concurrency for Intermittent Systems".

This work targets energy harvesting devices (e.g. low power sensors), so
substantial custom hardware is required to reproduce the conclusions drawn in
the paper. To enable artifact reviewers to feasibly evaluate the system, we have
set up all of the necessary hardware to accurately measure application
performance on a server in our lab. The server can be controlled
remotely via ssh.

This document provides instructions for accessing the server and executing the
scripts that automate the programming and testing of applications on a real
energy harvesting sensor board.

*Note* The artifact reviewer will be manipulating *real* hardware. This
introduces the possibility for "loose wire" issues and undefined behavior if
hardware is accessed without care. The reviewers should contact the authors
immediately if hardware is unresponsive and only access hardware using the
provided scripts. 

Getting Started
===============

## Connect to the Server

  Login credentials: user 'xxx' password 'xxx'

  Host Name: xxxx

  Use SSH to connect to the server and enter the password when prompted.
  E.g.  on Linux:

  $ ssh xxx@xxx

  *Note* Please communicate with the other artifact reviewers to ensure that
  only one reviewer is accessing the server at a time. There is only one sensor
  board and measurement setup and it cannot accomodate multiple users.

## Environment setup

  The directory 'coati_test' contains all of the application code, runtime code,
  test scripts and expected output of the applications. The necessary dependencies
  are all installed on the server and the bld directories within each app should
  be configured, but run the following commands to ensure that they are configured
  properly:

  $ cd ~/coati_test
  $ make apps/coati/bld/gcc
  $ make apps/buffi/bld/gcc
  $ make apps/alpaca/bld/alpaca

  Alpaca uses an LLVM pass to transform code so that it is safe for intermittent
  execution, to ensure that the pass if built, run the following commands:

  $ cd ~/coati_test/tools/alpaca/LLVM
  $ mkdir -p build
  $ cd build
  $ cmake ..
  $ make

## Testing applications

  The hardware setup is somewhat brittle, please use only the scripts provided
  in the 'coati_test/scripts' directory to test the applictions. These scripts
  will build, flash, and run all of the applications presented in the paper's
  evaluation, report the runtime, the percent of events captured, and the diff
  of the output with the expected output.

  run_alpaca_apps.sh
  run_coati_apps.sh
  run_buffi_apps.sh

  To run the scripts, provide the following argument:
    -1 or 0:  test on continuous power or harvested energy respectively 

  For example, from the 'coati_test' direcotry the following command runs all of
  the coati apps on continuous power:

  $ ./scripts/run_coati_apps.sh 1

  And this command runs the app on harvested energy:

  $ ./scripts/run_coati_apps.sh 0

Detailed Explanations
===================

## Test Directory

  The directory 'coati_test' contains all of the application code, runtime code,
  test scripts and expected output of the applications. The hierarchy of
  'coati_test' as it pertains to this evaluation is as follows:

+ Makefile : contains all of the configuration setttings
+ apps     : directory with application code for different system configurations
+--+ alpaca   : applications from the paper's eval that will be built with alpaca
   +--+ src  : contains source code for the different apps
   +--+ bld
      +--+ alpaca : this subdirectory is built by maker and contains the final
                    executable 
+--+ buffi  : contains the same test applications written for buffi
   +--+ src  : contains source code for the different apps
   +--+ bld
      +--+ gcc : this subdirectory is built by maker and contains the final
                 executable. Coati and buffi use gcc to compile, not a
                 specialized pass 
+--+ coati  : contains the same test applications written for coati
   +--+ src  : contains source code for the different apps
   +--+ bld
      +--+ gcc : this subdirectory is built by maker and contains the final
                 executable  
+ ext      : libraries required by the application
+--+ libcoatigcc : directory with source code for coati and buffi
+ tools    : contains maker code for building apps
+--+ maker : custom build system handles compiling app dependences
+--+ alpaca : contains the runtime code and LLVM pass that comprise alpaca
+ scripts  : all scripts for building, programming and evaluating applications
+ expected_outputs : the correct output for each of the test apps

## Hardware

  The following is an explanation of the hardware that the server communicates
  with directly:

  *Capybara board: The energy harvesting platform that all of the applications
  will run on. It contains an MSP43FR5994 microcontroller and a variety of low
  peripheral devices (IMU, magnetometer, gesture detector, BLE radio chip) as well
  as an energy harvesting power system.

  *UART to USB adapter: captures print statements produced by Capybara and relays
  them to the serial port /dev/ttyUSB0 on the server

  *MSP-FET MSP MCU Programmer: A programmer that will flash the executable onto
  the Capybara MCU. It communicates over serial ports /dev/ttyACM0 and /dev/ttyACM1

  *Arduino: An extra MCU for controlling the magnetic relay that electrically
  isolates the Capybara from the MSP-FET when Capybara is running on harvested
  energy and providing the gpio pulses that trigger events in the test apps. It
  communicates on serial port /dev/ttyACM2

  *Saleae Logic analyzer:  A digital logic analyzer that records a trace of gpio
  pulses and produced by Capybara while applications are running.

  See the labeled image included with the artifact for a full picture of the setup
  and the connections.


## Detailed Testing

  * Time to failure using alpaca:

  The paper demonstrates that the Alpaca programming model cannot be used for
  applications that have interrupt service routines that modify memory shared with
  synchronous tasks. When run_alpaca_apps.sh runs, it will check if the
  application execution encountered and error, if an error is encountered, instead
  of runtime it reports time to failure.

  To observe the failures that occur on harvestd energy, as shown in Table 3 of
  the paper, run:

  $ ./scripts/run_alpaca_apps.sh 0

  * Comparing buffi and coati:

  The paper demonstrated that the split-phase event strategy employed by Coati
  is more efficient than the full buffering used by Buffi. To easily compare the
  runtime difference on continuous power (as seen in Figure 12):

  $ ./scripts/run_coati_apps.sh 1
  $ ./scripts/run_buffi_apps.sh 1
  $ python scripts/compare_cont_pwrd_runtimes.py
  $ vim scripts/outputs/cont_pwrd_runtimes.csv

  Similarly, to compare the runtime on harvested energy (as seen in Figure 14),
  run:

  $ ./scripts/run_coati_apps.sh 0
  $ ./scripts/run_buffi_apps.sh 0
  $ python scripts/compare_harv_pwr_runtimes.py
  $ vim scripts/outputs/harv_pwr_runtimes.csv


