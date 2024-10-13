import terminal as term
import serialpi as sp
import display as dp
import time
import argparse

if __name__ == "__main__":
    print("================================================================================================")
    print(f">> Starting serial communication ... ")
    print("================================================================================================")
    # Part to be checked by executing python main.py --port COM13 --baudrate 115200
    # Extract parameters from command line or use default values for port and baudrate
    parser = argparse.ArgumentParser(description= 'Starting serial communication... ')
    parser.add_argument('--port', type=str, default='COM13',help='Serial port name (e.g., COM13)' )
    parser.add_argument('--baudrate', type=int, default=115200, help='Data transfer speed')
    args = parser.parse_args()
    # Check if parsed baudrate is in mapped serial baudrates dictionary. If not, use default baudrate
    if args.baudrate not in term.SERIAL_BAUDRATE:
        print(f"Invalid baudrate: {args.baudrate}. Using default baudrate: 115200.")
        args.baudrate = 115200
    ser = sp.serialpi(port=args.port, baudrate=args.baudrate)
    ser.open_port()
    try:
        while True:
            print(">> Input parameters to send via serial")
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
        print("Program terminated by user.")
        print("================================================================================================")
        print(">> Closing serial communication ... ")
        print("================================================================================================")
    finally:
        ser.close_port()
        print("================================================================================================")