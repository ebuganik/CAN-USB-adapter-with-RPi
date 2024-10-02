import terminal as term
import json

def print_input(method,json_string):
    if method == "write":
        json_list = json.loads(json_string)
        can_id = json_list.get("can_id")
        bitrate = json_list.get("bitrate")
        dlc = json_list.get("dlc")
        payload = json_list.get("payload")
        payload_strip = payload.strip("[]")
        payload_bytes = [int(byte, 16) for byte in payload_strip.split(",")]
        payload_str = " ".join(f"0x{byte:x}" for byte in payload_bytes)
        # Check if cycle time in miliseconds is specified
        if json_list.get("cycle_ms") == None:
            cycle = 0
        else: cycle = json_list.get("cycle_ms")
        
        print("=======================================================================================================================================")
        print(f"SENT DATA: {method} request")
        print(f"interface: {'can0'} bitrate: {bitrate} can_id: {can_id} dlc: {dlc} payload: {payload_str} cycle_ms: {cycle} count: {term.write_dict[json_string]}") 
    else:
        json_list = json.loads(json_string)
        bitrate = json_list.get("bitrate")
        if json_list.get("can_id") == None:
            print("=======================================================================================================================================")
            print(f"SENT DATA: {method} request")
            print(f"interface: {'can0'} bitrate: {bitrate}") 
        else:
            can_id = json_list.get("can_id")
            can_mask = json_list.get("can_mask")
            can_id_strip = can_id.strip("[]")
            can_mask_strip = can_mask.strip("[]")
            can_id_bytes = [int(byte, 16) for byte in can_id_strip.split(",")]
            can_mask_bytes = [int(byte, 16) for byte in can_mask_strip.split(",")]
            can_id_str = " ".join(f"0x{byte:x}" for byte in can_id_bytes)
            can_mask_str = " ".join(f"0x{byte:x}" for byte in can_mask_bytes)
            print("=======================================================================================================================================")
            print(f"SENT DATA: {method} request")
            print(f"interface: {'can0'} bitrate: {bitrate} can_id: {can_id_str} can_mask: {can_mask_str}") 
            


def print_output(json_string):
    # Read data from CAN bus is packed into JSON string
    if "{" in json_string:
        # Convert JSON into dictionary
        json_list = json.loads(json_string)
        can_id = json_list.get("can_id")
        formatted_dict_id = f'{{"can_id": "{can_id}"}}'
        dlc = json_list.get("dlc")
        payload = json_list.get("payload")
        payload_strip = payload.strip("[]")
        payload_bytes = [int(byte, 16) for byte in payload_strip.split(",")]
        payload_str = " ".join(f"0x{byte:x}" for byte in payload_bytes)
        print("=======================================================================================================================================")
        print("RECEIVED DATA:")
        print(f"interface: {'can0'} can_id: {can_id} dlc: {dlc} payload: {payload_str} cycle_ms: {term.check_r_count(formatted_dict_id):.2f} count: {term.read_dict[formatted_dict_id]}")
        print("=======================================================================================================================================")

    else:
        # Information about success or state of a CAN node is received as a normal string
        print("=======================================================================================================================================")
        print("RECEIVED DATA:")
        print(json_string)
        print("=======================================================================================================================================")
