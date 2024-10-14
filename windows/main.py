import terminal as term
import serialpi as sp
import display as dp
import time
import argparse

if __name__ == "__main__":
    
    # Extract parameters from command line or use default values for port and baudrate
    parser = argparse.ArgumentParser(description= 'Serial communication parameters input ')
    parser.add_argument('--port', type=str, default='COM13', help='Serial port name (e.g., COM13)' )
    parser.add_argument('--baudrate', type=int, default=115200, help='Data transfer speed')
    args = parser.parse_args()
    # Check if parsed baudrate is in mapped serial baudrates dictionary. If not, use default baudrate
    if args.baudrate not in term.SERIAL_BAUDRATE:
        args.baudrate = 115200
        print(f"\n\nInvalid baudrate: {args.baudrate}. Using default baudrate: {args.baudrate}.")
    # Check if parsed serial port is in serial ports list. If not, request new input 
    while not sp.is_valid_port(args.port):
        print(f"Port {args.port} is not valid. Available ports: {', '.join(sp.get_serial_ports())}")
        args.port = input("Please enter a valid serial port: ")
    try:
        ser = sp.SerialPi(args.port, args.baudrate)
        ser.open_port()
        term.show_manual()
        while True:
            json_output = term.read_input()
            if(json_output):
                val = ser.write_serial(json_output)
            while True:
                data = ser.read_serial()
                if data:
                    dp.print_output(data)
                    break
                time.sleep(0.1)
    except KeyboardInterrupt:
        print("\nProgram terminated by user.")
    finally:
        ser.close_port()