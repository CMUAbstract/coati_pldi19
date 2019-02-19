Overview
========

This document explains the runtime system used in PLDI 2019 paper submission #9:
"Transactional Concurrency Control for Intermittent, Energy-Harvesting Computing
Systems".

This work targets energy harvesting devices (e.g. low power sensors), so
substantial custom hardware is required to reproduce the conclusions drawn in
the paper.  To test the code in this repo, you will need:
*MCU: TI MSP430FR5994 MCU or TI MSP430FR5994 launchpad
*Programmer: Standalone MSPFET or the FET built into the launchpad)
*UART to USB converter: UART-USB connecter rigged up over a level shifter or the UART
built into the launchpad.
*External GPIO trigger: a means of and a means of toggling gpio pin P3.5 on
the TI MCU (we used an Arduino connected over a level shifter).

For more details on putting together a setup for testing on harvested energy,
see the "artfact" branch of this repo.


Getting Started
===============
## Clone the repo and all its submodules

  git clone  --recursive
  git submodule update

## Environment setup
  Change the TI_ROOT, LLVM_ROOT and CLANG_ROOT paths in tools/maker/Makefile.env
  to reflect your system's paths. Our system set up uses the following versions:
  mspgcc 7.3.2
  llvm 7.0.1
  clang 7.0.1

  Run the following commands to make sure the build directories exist

  $ make apps/coati/bld/gcc
  $ make apps/buffi/bld/gcc
  $ make apps/alpaca/bld/alpaca

  Alpaca uses an LLVM pass to transform code so that it is safe for intermittent
  execution, to ensure that the pass if built, run the following commands:

  $ cd tools/alpaca/LLVM
  $ mkdir -p build
  $ cd build
  $ cmake ..
  $ make

## Building applications

  The following scripts in ./scripts make it easy to build applications and
  their dependences with different toolchains (alpaca, coati or buffi)

  run_alpaca_app.sh
  run_coati_app.sh
  run_buffi_app.sh

  To run the scripts, provide the following argument:
    -1 or 0:  test on continuous power or harvested energy respectively*
    -<app name> choose from the available apps: bc, ar, cem, cuckoo, blowfish,
    rsa
  *Note, if you are not running on a Capybara board using harvested energy this
  must be set to 1 for the code to work correctly

  For example, from the top level direcotry the following command builds and
  runs bitcount (bc) on continuous power.

  $ ./scripts/run_coati_apps.sh 1 bc

Extra Explanations
===================

## Test Directory

  The top levle directory contains all of the application code, runtime code,
  test scripts and expected output of the applications. The hierarchy of
  as it pertains to this paper's evaluation is as follows:

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



