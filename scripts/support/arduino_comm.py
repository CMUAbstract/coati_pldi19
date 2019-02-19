import os
import sys
import serial

# arguments to this script:
# -serial port where arduino is
# -letter to send out

ser = serial.Serial(sys.argv[1], 9600, timeout=None)

data = sys.argv[2]
byte = bytes(data, 'utf-8')
ser.write(byte)

