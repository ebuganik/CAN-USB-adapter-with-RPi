import serial
import time
import json

class SerialPi:
    def __init__(self, port, baudrate, parity = "N", stopbits = 1, bytesize = 8, timeout = 0, write_timeout = 2):
        self.port = port
        self.baudrate = baudrate
        self.parity = parity
        self.stopbits = stopbits
        self.bytesize = bytesize
        self.timeout = timeout
        self.write_timeout = write_timeout
        self.ser = serial.Serial(
           port = self.port,
           baudrate = self.baudrate,
           timeout = self.timeout,
           parity = self.parity,
           stopbits = self.stopbits,
           bytesize = self.bytesize,
           write_timeout = self.write_timeout
        )

    def open_port(self):
        try:
            if not self.ser.is_open:
                self.ser.open()
                print(f"Serial port {self.ser.portstr} is opened.")
            if self.ser.is_open:
                print(f"Serial port {self.ser.portstr} is opened.")
            else:
                print(f"Serial port {self.ser.portstr} isn't opened.")
        except serial.SerialException as e:
            print(f"Error opening serial port {self.port}: {e}")

    def write_serial(self, json_output):
        if self.ser.is_open:
            try:
                val = self.ser.write(json_output.encode("utf-8"))
                print(f"Bytes written: {val}")
            except serial.SerialException as e:
                print(f"Error writing to serial port: {e}")
        else:
            print("Serial port isn't opened")

    def read_serial(self):
        if self.ser and self.ser.is_open:
            try:
                self.ser.in_waiting > 0
                data = self.ser.readline().decode("utf-8").strip()
                print_data(data)
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
            
def print_data(json_string):
    
    # Read data from CAN bus is packed into JSON string
    if "{" in json_string:
        # Convert JSON into dictionary
        json_list = json.loads(json_string)
        can_id = json_list.get("can_id")
        dlc = json_list.get("dlc")
        payload = json_list.get("payload")
        payload_strip = payload.strip("[]")
        payload_bytes = [int(byte, 16) for byte in payload_strip.split(",")]
        payload_str = " ".join(f"0x{byte:x}" for byte in payload_bytes)
        print("================================================================================================")
        print("RECEIVED DATA:")
        print("================================================================================================")
        print(f"interface: {'can0'} can_id: {can_id} dlc: {dlc} payload: {payload_str}")
        print("================================================================================================")

    else:
        # Information about success or state of a CAN node is received as a normal string
        print("================================================================================================")
        print("RECEIVED DATA:")
        print("================================================================================================")
        print(json_string)
        print("================================================================================================")
