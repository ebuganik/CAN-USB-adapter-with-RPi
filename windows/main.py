import terminal as term
import serialpi as sp
import display as dp
import addition as add
import time

if __name__ == "__main__":
    port, baudrate = add.parse_n_check()
    try:
        ser = sp.SerialPi(port,baudrate)
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