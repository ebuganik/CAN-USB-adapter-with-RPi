import terminal as term
import json

def print_input(method,json_string):
    """
    Prints the input parameters for a CAN bus operation.

    Depending on the method ('write' or 'read'), this function parses the request JSON string
    and prints the relevant parameters for the CAN bus operation.

    Parameters:
    -----------
    method : str
        The method of operation ('write' or 'read').
    json_string : str
        The JSON string containing the parameters.

    Raises:
    -------
    ValueError
        If the JSON string cannot be parsed.
    """
    if method == "write":
        json_list = json.loads(json_string)
        can_id = json_list.get("can_id")
        bitrate = json_list.get("bitrate")
        dlc = json_list.get("dlc")
        payload = json_list.get("payload")
        payload_strip = payload.strip("[]")
        payload_bytes = [int(byte, 16) for byte in payload_strip.split(",")]
        payload_str = " ".join(f"0x{byte:x}" for byte in payload_bytes)
        if json_list.get("cycle_ms") == None:
            cycle = 0
        else: cycle = json_list.get("cycle_ms")
        
        print("=======================================================================================================================================")
        print(f"method: {method}")
        print(f"params:")
        print(f"\tbitrate: {bitrate} \n\tcan_id: {can_id} \n\tdlc: {dlc} \n\tpayload: {payload_str} \n\tcycle_ms: {cycle} \n\tcount: {term.write_dict[json_string]}") 
    else:
        json_list = json.loads(json_string)
        bitrate = json_list.get("bitrate")
        filtering = json_list.get("filter_enable")
        if filtering == "NO":
            print("=======================================================================================================================================")
            print(f"method: {method}")
            print(f"params:")
            print(f"\tbitrate: {bitrate} \n\tfilter_enable: {filtering}")
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
            print(f"method: {method}")
            print(f"params:")
            print(f"\tbitrate: {bitrate} \n\tfilter_enable: {filtering} \n\tcan_id: {can_id_str} \n\tcan_mask: {can_mask_str}") 
            


def print_output(json_string):
    """
    Prints the output response from a CAN bus operation.

    This function parses response in form of a JSON string and prints the relevant response parameters
    from the CAN bus operation.

    Parameters:
    -----------
    json_string : str
        The JSON string containing the response parameters.

    Raises:
    -------
    ValueError
        If the JSON string cannot be parsed.
    """
    print("response:")
    if "can_id" in json_string:
        json_list = json.loads(json_string)
        interface = json_list.get("interface")
        can_id = json_list.get("can_id")
        formatted_dict_id = f'{{"can_id": "{can_id}"}}'
        dlc = json_list.get("dlc")
        payload = json_list.get("payload")
        payload_strip = payload.strip("[]")
        payload_bytes = [int(byte, 16) for byte in payload_strip.split(",")]
        payload_str = " ".join(f"0x{byte:x}" for byte in payload_bytes)
        print(f"\tinterface: {'can0'} \n\tcan_id: {can_id} \n\tdlc: {dlc} \n\tpayload: {payload_str} \n\tcycle_ms: {term.check_r_count(formatted_dict_id):.2f} \n\tcount: {term.read_dict[formatted_dict_id]}")
        print("=======================================================================================================================================")

    else:
        json_list = json.loads(json_string)
        interface = json_list.get("interface")
        code = json_list.get("status_code")
        message = json_list.get("status_message")
        print(f"\tinterface: {interface} \n\tstatus_code: {code} \n\tstatus_message: {message}")
        print("=======================================================================================================================================")
