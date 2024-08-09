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
# Cannot be parsed the right way
# data = {{"method","read"},{"bitrate",250000},
#         {"id", "0x123"},{"dlc", 4},
#         {"data","[0x11,0x22,0x33,0x44]"}
#         }
# Unable to do this
# data_raw = R"({"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"{0x11,0x22,0x33,0x44}"})" 
# 1 - Able to do this
# data = {"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"}
# 2 - Able to do this and I probably will
data = 'R"({"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"})"'
# probati i sa ovim
#data +=str('\n')
# 3 - Able to to this but cannot parse 
# data = '{"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"}'
# data = json.dumps(data)

# print JSON string
print(data)
# Send over serial
while True:
    val=ser.write(data.encode('utf-8'))
    print(f"Bytes written: {val}")

