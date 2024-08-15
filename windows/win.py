#!/usr/bin/env python
import serial
import json
import time
ser = serial.Serial("COM13", 115200,
    parity=serial.PARITY_NONE,
    bytesize=serial.EIGHTBITS,
    stopbits=serial.STOPBITS_ONE,
    timeout=1
    )

# Creating a python object
# 2 - Able to do this and I probably will
json_out = {"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"}
# data = 'R"({"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"})"'
# probati i sa ovim
#data +=str('\n')
# 3 - Able to to this but cannot parse 
# data = '{"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"}'
# data = json.dumps(data)

# print JSON string
# print(data)
# Send over serial
while True:
    json_output = json.dumps(json_out)
    data = f'R"({json_output})"'
    data += '\n'
    print(data.encode('utf-8'))
    time.sleep(3)
    val=ser.write(data.encode('utf-8'))
    print(f"bytes written: {val}")
    #print(ser.readline())
    # Ovdje dodati provjeru da prilikom primanja stringa provjeri čime počinje i završava
