import serial
import serial.tools.list_ports

class SerialPi:
    def __init__(self, port, baudrate, parity = "N", stopbits = 1, bytesize = 8, timeout = 0, write_timeout = 2):
        self.ser = serial.Serial(
           port = port,
           baudrate = baudrate,
           timeout = timeout,
           parity = parity,
           stopbits = stopbits,
           bytesize = bytesize,
           write_timeout = write_timeout
        )

    def open_port(self):
        try:
            if not self.ser.is_open:
                self.ser.open()
                print(f"Serial port {self.ser.portstr} is opened.")
                print(f"Connected to {self.ser.portstr} at {self.ser.baudrate} baud.")
            if self.ser.is_open:
                print(f"Serial port {self.ser.portstr} is opened.")
                print(f"Connected to {self.ser.portstr} at {self.ser.baudrate} baud.")
            else:
                print(f"Serial port {self.ser.portstr} isn't opened.")
        except serial.SerialException as e:
            print(f"Error opening serial port {self.port}: {e}")
            
    def write_serial(self, json_output):
        if self.ser.is_open:
            try:
                val = self.ser.write(json_output.encode("utf-8"))
            except serial.SerialException as e:
                print(f"Error writing to serial port: {e}")
        else:
            print("Serial port isn't opened\n")

    def read_serial(self):
        if self.ser and self.ser.is_open:
            try:
                if self.ser.in_waiting > 0:
                    data = self.ser.readline().decode("utf-8").strip()
                    return data
                return None
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
            
def get_serial_ports():
    return [port.device for port in serial.tools.list_ports.comports()]

def is_valid_port(port):
    return port in get_serial_ports()
