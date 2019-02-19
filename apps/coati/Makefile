#TODO figure out if this is a problem
#DEPS += libmspmath

EXEC = coati_test
TEST ?= bc

OBJECTS = main_$(TEST).o

DEPS = libmspbuiltins:gcc libio:gcc \
       libcapybara:gcc libmspware:gcc \
       libfxl:gcc libmsp:gcc libcoatigcc:gcc libmspmath:gcc

include tools/maker/Makefile

