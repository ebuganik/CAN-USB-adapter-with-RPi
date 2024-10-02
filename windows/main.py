import terminal as term
import serialpi as sp
import display as dp
import time

if __name__ == "__main__":
    print("================================================================================================")
    print(f">> Starting serial communication ... ")
    print("================================================================================================")
    ser = sp.SerialPi(port = "COM13", baudrate = 115200)
    ser.open_port()
    print("================================================================================================")
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