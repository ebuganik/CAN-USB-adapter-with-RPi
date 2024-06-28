#!/usr/bin/env python
import serial
import json

ser = serial.Serial("COM11", 115200,
    parity=serial.PARITY_NONE,
    bytesize=serial.EIGHTBITS,
    stopbits=serial.STOPBITS_ONE,
    timeout=1
    )

# Creating a python object
# This cannot be parsed in right way
data = {{"method","read"},{"bitrate",250000},
        {"id", 123},{"dlc", 4},
        {"data","[0x11,0x22,0x33,0x44]"}
        }
# Unable to do this
# data_raw = R"({"method":"read","bitrate":250000,"id":123,"dlc":4,"data":"{0x11,0x22,0x33,0x44}"})" 
# data = {"method":"read","bitrate":250000,"id":123,"dlc":4,"data":"[0x11,0x22,0x33,0x44]"}
data = json.dumps(data)+'\n'

# print JSON string
print(data)
# Send over serial
while True:
    ser.write(data.encode('utf-8'))

