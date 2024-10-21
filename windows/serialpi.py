import serial
import serial.tools.list_ports

class SerialPi:
    """
    A class to manage serial communication with Raspberry Pi.

    Attributes:
    -----------
    ser : serial.Serial
        The serial connection object.

    Methods:
    --------
    open_port():
        Opens the serial port if it is not already open or checks if it is.
    write_serial(json_output):
        Writes a request in form of JSON string to the serial port.
    read_serial():
        Reads a line of data from the serial port.
    close_port():
        Closes the serial port if it is open.
    """
    def __init__(self, port, baudrate, parity = "N", stopbits = 1, bytesize = 8, timeout = 0, write_timeout = 2):
        """
        Initializes the SerialPi object with the specified parameters.

        Parameters:
        -----------
        port : str
            The serial port to connect to.
        baudrate : int
            The baudrate for the serial communication. Possible baudrates specified in SERIAL_BAUDRATE dictionary.
        parity : str, optional
            The parity bit setting (default is 'N' for None).
        stopbits : int, optional
            The number of stop bits (default is 1).
        bytesize : int, optional
            The number of data bits (default is 8).
        timeout : int, optional
            The read timeout value in seconds (default is 0).
        write_timeout : int, optional
            The write timeout value in seconds (default is 2).
        """
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
        """
    Opens the serial port if it is not already open or checks if it is.

    This method attempts to open the serial port specified during the initialization
    of the SerialPi object. If the port is successfully opened, it prints a confirmation
    message with the connected port name and at which baudrate. If there is an error while opening the port,
    it catches the SerialException and prints an error message.

    Raises:
    -------
    serial.SerialException
        If there is an error opening the serial port.
    """
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
        """
        Writes a request in form of JSON string to the serial port.

        This method writes the provided JSON string to the serial port. If the port is not open,
        it prints a message indicating that the port is not open. If there is an error while writing
        to the port, it catches the SerialException and prints an error message.

        Parameters:
        -----------
        json_output : str
            The JSON string to write to the serial port.

        Raises:
        -------
        serial.SerialException
            If there is an error writing to the serial port.
        """
        if self.ser.is_open:
            try:
                val = self.ser.write(json_output.encode("utf-8"))
            except serial.SerialException as e:
                print(f"Error writing to serial port: {e}")
        else:
            print("Serial port isn't opened\n")

    def read_serial(self):
        """
        Reads a line of data from the serial port.

        This method reads a line of data from the serial port if data is available. If the port is not open,
        it prints a message indicating that the port is not open. If there is an error while reading from the port,
        it catches the SerialException and prints an error message.

        Returns:
        --------
        str or None
            The data read from the serial port, or None if no data is available.

        Raises:
        -------
        serial.SerialException
            If there is an error reading from the serial port.
        """
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
        """
        Closes the serial port if it is open.

        This method closes the serial port if it is currently open. If the port is already closed,
        it prints a message indicating that the port is already closed. If there is an error while closing
        the port, it catches the SerialException and prints an error message.

        Raises:
        -------
        serial.SerialException
            If there is an error closing the serial port.
        """
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
                print(f"Serial port {self.ser.portstr} is closed.")
            except serial.SerialException as e:
                print(f"Error closing serial port: {e}")
        else:
            print(f"Serial port {self.ser.portstr} is already closed")
            
def get_serial_ports():
    """
    Returns a list of available serial ports.

    This function retrieves a list of all available serial ports on the system.

    Returns:
    --------
    list of str
        The list of available serial ports.
    """
    return [port.device for port in serial.tools.list_ports.comports()]

def is_valid_port(port):
    """
    Checks if the specified port is a valid serial port.

    This function checks if the provided port is in the list of available serial ports.

    Parameters:
    -----------
    port : str
        The serial port to check.

    Returns:
    --------
    bool
        True if the port is valid, False otherwise.
    """
    return port in get_serial_ports()
