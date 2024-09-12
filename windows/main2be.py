import terminal as term
import serialpi as sp
import time

if __name__ == "__main__":
    print("================================================================================================")
    print(" >>> Starting serial communication ")
    print("================================================================================================")
    ser = sp.SerialPi(port = "COM13", baudrate = 115200)
    ser.open_port
    try:
        while True:
            json_output = term.read_input()
            if json_output:
                print(json_output)
            ser.write_serial(json_output)
            while True:
                data = ser.read_serial()
                if data:
                    sp.print_data(data)
                    break
                time.sleep(0.1)
    except KeyboardInterrupt:
        print("Keyboard Interrupt by user.")

    finally:
        ser.close_port
