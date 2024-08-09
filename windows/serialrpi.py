import serial
import json

class SerialPi:
    def __init__(self, port, baudrate, timeout = 1, parity = "N", stopbits = 1, bytesize = 8):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.parity = parity
        self.stopbits = stopbits
        self.bytesize = bytesize
        self.ser = serial.Serial(self.port, self.baudrate, self.timeout, self.parity, self.stopbits, self.bytesize)
    def open_port(self):
        try:
            if not self.ser.is_open():
                self.ser.open()
            if self.ser.is_open():
                print(f"Serial port {self.ser.portstr} is opened.")
            else:
                print(f"Serial port {self.ser.portstr} isn't opened.")
        except serial.SerialException as e:
            print(f"Error opening serial port {self.port}: {e}")
    def write(self, string):
        if self.ser and self.ser.is_open():
            try:
                self.ser.write(string.encode('utf-8'))
            except serial.SerialException as e:
                print(f"Error writing to serial port: {e}")
        else:
            print("Serial port isn't opened")
    def read(self):
        if self.ser and self.ser.is_open:
            try:
                if self.ser.in_waiting > 0:
                    data = self.ser(self.ser.in_waiting).decode('utf-8')
                    return data
            except serial.SerialException as e:
                print(f"Error reading from serial port: {e}")
        else:
            print("Serial port isn't opened.")
                   
    def close_port(self):
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
                print(f"Serial port {self.ser.portstr} is closed.")
            except serial.SerialException as e:
                print(f"Error closing serial port: {e}")
        else:
            print(f"Serial port {self.ser.portstr} is already closed")
            
if __name__ == "__main__":
    ser = SerialPi(port = 'COM10', baudrate=115200)   
    ser.open_port()
    data = 'R"({"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"})"'
    try:
         
        while True:
            ser.write(data)
        # Check if string came, ending with '}'    
    finally:
        ser.close_port()
                