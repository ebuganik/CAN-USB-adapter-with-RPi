import terminal as term
import serial
import json
import time


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


if __name__ == "__main__":
    # Constructor to be
    ser = serial.Serial(
        "COM13",
        115200,
        parity=serial.PARITY_NONE,
        bytesize=serial.EIGHTBITS,
        stopbits=serial.STOPBITS_ONE,
        timeout=0,
        writeTimeout=2,
    )
    try:
        while True:
            json_output = term.read_input()
            if json_output:
                print(json_output)    
            val = ser.write(json_output.encode("utf-8"))
            print(f"Bytes written: {val}")

            while True:
                # Function read_serial
                if ser.in_waiting > 0:
                    data = ser.readline().decode("utf-8").strip()
                    print_data(data)
                    break
                time.sleep(0.1)
                
        
    except serial.SerialException as e:
        print(f"Error: {e}")

    finally:
        ser.close()