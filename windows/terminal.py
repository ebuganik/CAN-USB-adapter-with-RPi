import json
import serial
import time

# Map possible range of bitrates for testing PEAKCAN USB
MIN_BITRATE = 5000     # Min bitrate
MAX_BITRATE = 1000000  # Max bitrate

def read_input():
    while True:
        method = input("Enter method (write or read): ").strip().lower()
        if method in ["write", "read"]:
            break
        else:
            print("Method unknown. Please enter 'write' or 'read'.")
    
    json_data = {"method": method}
                    
    if method == "write":
        try:
            # CAN ID input
            can_id = input("Enter CAN ID as hex value: ").strip()
            can_id = int(can_id, 16)
            if not (0 <= can_id <= 2047):  # Check Standard CAN ID range 
                raise ValueError("Not a valid CAN ID.")
            formatted_can_id = f"0x{can_id:03X}"
            
            # Input desired bitrate
            bitrate = int(input("Enter desired bitrate: ").strip())
            if not MIN_BITRATE <= bitrate <= MAX_BITRATE:
                raise ValueError(f"Bitrate must be between {MIN_BITRATE} and {MAX_BITRATE} bps.")
            
            # DLC
            dlc = int(input("Enter CAN DLC (Data Length Code): ").strip())
            if not (0 <= dlc <= 8):  # DLC range 
                raise ValueError("Not a valid CAN DLC.")

            # Payload input check
            while True:
                payload = input("Input payload hex bytes separated by spaces e.g. 11 22 33: ").strip()
                payload_bytes = payload.split()
                if len(payload_bytes) != dlc:
                    print(f"Error: Payload length does not match DLC. Expected {dlc} bytes, got {len(payload_bytes)} bytes.")
                    continue
                
                # Format payload bytes
                try:
                    pay_hex = [int(b, 16) for b in payload_bytes]
                    formatted_payload_bytes = [f"0x{byte:02X}" for byte in pay_hex]
                    formatted_payload = "[" + ",".join(formatted_payload_bytes) + "]"
                except ValueError:
                    print("Error: All bytes must be valid hexadecimal values.")
                    continue
                break
        
            # Pack data into JSON
            json_data.update({
                "can_id": formatted_can_id,
                "bitrate": bitrate,
                "dlc": dlc,
                "payload": formatted_payload
            })
       
        except ValueError as e:
            print(f"Error: {e}")
            return None
        
    # Needs to be done
    # In case of filtering, add can_id and mask 
    elif method == "read":
        try:
             # CAN ID input
            can_id = input("Enter CAN ID as hex value: ").strip()
            can_id = int(can_id, 16)
            if not (0 <= can_id <= 2047):  # Check Standard CAN ID range 
                raise ValueError("Not a valid CAN ID.")
            formatted_can_id = f"0x{can_id:03X}"
            
            # Input desired bitrate
            bitrate = int(input("Enter desired bitrate: ").strip())
            if not MIN_BITRATE <= bitrate <= MAX_BITRATE:
                raise ValueError(f"Bitrate must be between {MIN_BITRATE} and {MAX_BITRATE} bps.")
            
            # Pack data into JSON
            json_data.update({
                "can_id": formatted_can_id,
                "bitrate": bitrate
            }) 
            
        except ValueError as e:
            print(f"Error: {e}")
            return None 

    j = json.dumps(json_data)
    json_send = f'R"({j})"'
    json_send += '\n'
    return json_send


# TEST
if __name__ == "__main__":
    ser = serial.Serial("COM13", 115200,
    parity=serial.PARITY_NONE,
    bytesize=serial.EIGHTBITS,
    stopbits=serial.STOPBITS_ONE,
    timeout = 0, 
    writeTimeout = 2 
    )
    try:
        while True:
            json_output = read_input()
            if json_output:
                print(json_output)
       
    # To send via serial data should be packed as 'R"({"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"})"'
            val=ser.write(json_output.encode('utf-8'))
            print(f"bytes written: {val}")
            while True:
                if ser.in_waiting > 0:
                    data = ser.readline().strip()
                    print(f"Received data: {data}")
                    break
                time.sleep(0.1) 
                
    except serial.SerialException as e:
            print(f"Error: {e}")

    finally:
        ser.close() 
        
