import argparse
import serialpi as sp

# Mapping supported bitrates for testing with PEAKCAN-USB (CAN SJA-1000 mode)

PCAN_BITRATES = {
    5000: "5 kbps",
    10000: "10 kbps",
    20000: "20 kbps",
    33333: "33.333 kbps",
    40000: "40 kbps",
    47619: "47.619 kbps",
    50000: "50 kbps",
    83333: "83.333 kbps",
    95238: "95.238 kbps",
    100000: "100 kbps",
    125000: "125 kbps",
    200000: "200 kbps",
    250000: "250 kbps",
    500000: "500 kbps",
    800000: "800 kbps",
    1000000: "1 Mbps",
}

# Mapping supported baudrates for serial communication

SERIAL_BAUDRATE = {
     50   :'B50',
     75   :'B75',
     110  :'B110',
     134  :'B134',
     150  :'B150',
     200  :'B200',
     300  :'B300',
     600  :'B600',
     1200 :'B1200',
     1800 :'B1800',
     2400 :'B2400',
     4800 :'4800',
     9600 : '9600',
     19200 : '19200',
     38400 : '38400',
     57600 : '57600',
     115200 : '115200',
     230400 : '230400',
     460800 : '460800',
     500000 : '500000',
     576000 : '576000',
     921600 : '921600',
     1000000 : '1000000',
     1152000 : '1152000',
     1500000 : '1500000',
     2000000 : '2000000',
     2500000 : '2500000',
     3000000 : '3000000',
     3500000 : '3500000',
     4000000 : '4000000'
}

def parse_n_check():
     # Extract parameters from command line or use default values for port and baudrate
    parser = argparse.ArgumentParser(description= 'Serial communication parameters input ')
    parser.add_argument('--port', type=str, default='COM13', help='Serial port name (e.g., COM13)' )
    parser.add_argument('--baudrate', type=int, default=115200, help='Data transfer speed')
    args = parser.parse_args()
    # Check if parsed baudrate is in mapped serial baudrates dictionary. If not, use default baudrate
    if args.baudrate not in SERIAL_BAUDRATE:
        args.baudrate = 115200
        print(f"\n\nInvalid baudrate: {args.baudrate}. Using default baudrate: {args.baudrate}.")
    # Check if parsed serial port is in serial ports list. If not, request new input 
    while not sp.is_valid_port(args.port):
        print(f"Port {args.port} is not valid. Available ports: {', '.join(sp.get_serial_ports())}")
        args.port = input("Please enter a valid serial port: ")
    return args.port,args.baudrate
