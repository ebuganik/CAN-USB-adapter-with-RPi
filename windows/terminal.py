import display as dp
import addition as add
import json
import time

"""
write_dict : dict
    Dictionary to store information about written frames and their counts.
read_dict : dict
    Dictionary to store information about read frames and their counts.
read_dict_cycle : dict
    Dictionary to store timestamps of read frames for calculating time intervals.
"""
write_dict = {}
read_dict = {}
read_dict_cycle = {}

def show_manual():
    """
    Opens and displays the user manual from 'manual.txt'.

    This function attempts to open the 'manual.txt' file and print its contents.

    Raises:
    -------
    FileNotFoundError
        If the file is not found, it prints an error message.
    """
    try:
        with open('manual.txt', 'r') as file:
            manual_text = file.read()
            print(manual_text)
    except FileNotFoundError:
        print("Error: Manual file not found.")

def method_check(method):
    """
    Verifies if a write or read request has already been sent before.
    This function checks if the specified method is present in the write_dict.

    Parameters:
    -----------
    method : str
        The method to check ('write' or 'read').

    Returns:
    --------
    bool
        True if the method is found in write_dict, False otherwise.
    """
    for key in reversed(write_dict):
        if method in key:
            return True
    return False

def repeat_request(method):
    """
    Returns the previous write/read request to be executed again.

    This function searches for the last request of the specified method in write_dict
    and updates the count for that request.

    Parameters:
    -----------
    method : str
        The method to repeat ('write' or 'read').

    Returns:
    --------
    str
        The last found request string for the specified method.
    """
    for key in reversed(write_dict):
        if method in key:
            check_w_count(key)
            return key

def check_w_count(write_string):
    """
    Checks write requests and updates the value of the counters.

    This function increments the count for the specified write request in write_dict.
    If the request is not already in the dictionary, it adds it with a count of 1.

    Parameters:
    -----------
    write_string : str
        The write request string to check.
    """
    if write_string in write_dict:
        write_dict[write_string] += 1
    else:
        write_dict[write_string] = 1


def check_r_count(formatted_can_id):
    """
    Checks read frames by the can_id field, calculates the time interval between two read frames with the same can_id, and updates the value of the counters.

    This function increments the count for the specified can_id in read_dict and calculates
    the time interval since the last occurrence. It updates the timestamp in read_dict_cycle.

    Parameters:
    -----------
    formatted_can_id : str
        The formatted CAN ID to check.

    Returns:
    --------
    float
        The time interval in milliseconds since the last occurrence of the specified can_id.
    """
    current_time = time.time()
    if formatted_can_id in read_dict:
        read_dict[formatted_can_id] += 1
        time_interval = (current_time - read_dict_cycle[formatted_can_id]) * 1000
        read_dict_cycle[formatted_can_id] = current_time
        return time_interval
    else:
        read_dict[formatted_can_id] = 1
        read_dict_cycle[formatted_can_id] = current_time
        return 0

def read_input():
    """
    Inputs write/read request parameters and combines them into a JSON string.

    This function prompts the user to enter a operation command ('write' or 'read' and the corresponding
    parameters) or CTRL+C to interrupt application and '--man' to read user manual.
    It formats the parameters into a JSON string for serial communication, so chosen request
    parameters are displayed later to the user before sending the request to serial port.

    The function also allows user to repeat previous 'read' or 'write' request if exists. Also,
    user is able to select the bitrate from the available bitrate values in PEAKCAN_BITRATES dictionary.

    JSON string format for 'write' request:
    ---------------------------------------
    {
        "method": "write",
        "bitrate": <bitrate>,
        "can_id": "0x<can_id>",
        "dlc": <dlc>,
        "payload": "[0x<byte1>, 0x<byte2>, ...]",
        "cycle_ms": <cycle_time> (optional)
    }

    To send a write request via serial, JSON string should be packed as following:
    -----------------------------------------------
    'R"({"method":"write","bitrate":<bitrate>,"can_id":"0x<can_id>","dlc":<dlc>,"payload":"[0x<byte1>,0x<byte2>,0x<bytes3>]"})"'


    JSON string format for 'read' request:
    --------------------------------------
    {
        "method": "read",
        "bitrate": <bitrate>,
        "can_id": "[0x<can_id1>, 0x<can_id2>, ...]" (optional),
        "can_mask": "[0x<mask1>, 0x<mask2>, ...]" (optional)
    }

    To send a read request via serial, JSON string should be packed as following:
    ----------------------------------------------
    'R"({"method":"read","bitrate":<bitrate>})"'

    To send a read request with enabled filtering via serial, JSON string should be packed as following:
    --------------------------------------------------------------------------------
    'R"({"method":"read","bitrate":<can_id>, "can_id": "[0x<can_id1>, 0x<can_id2>]", "can_mask": "[0x<mask1>, 0x<mask2>]"})"'

    Returns:
    --------
    str or None
        The formatted JSON string for the request, or None if there is an error.

    Raises:
    -------
    ValueError
        If there is an error in the input values.
    """
    while True:
        command = (
            input("Enter command: ").strip().lower()
        )
        if command in ["write", "read"]:
            break
        elif command == '--man':
            show_manual()
        else:
            print(f"Unknown command: {command}. Use '--man' for assistance.")

    json_data = {"method": command}


    if command == "write":
        try:
            if write_dict.__len__() > 0:
                if method_check(command):
                    repeat = input("Repeat last write request? (Y/N): ").strip().upper()
                    if repeat == "Y":
                        res_w = repeat_request(command)
                        json_send = f'R"({res_w})"'
                        json_send += "\n"
                        return json_send

            pcan_bitrates_str = "\n ".join(
                [f"{rate} bps ({add.PCAN_BITRATES[rate]})" for rate in add.PCAN_BITRATES]
            )
            print(f"Supported bitrates:\n {pcan_bitrates_str}")

            while True:
                try:
                    bitrate = int(input("Enter desired bitrate: ").strip())
                    if bitrate not in add.PCAN_BITRATES:
                        raise ValueError(f"Unsupported bitrate. Choose one of the supported bitrates.")
                    break
                except ValueError as e:
                    print(f"Error: {e}")

            while True:
                try:
                    frame_type = input("Standard format frame (type 'SFF') or Extended (type 'EFF') or Remote Transfer Frame (type 'RTR')? ").strip().upper()
                    if frame_type not in ["SFF", "EFF", "RTR"]:
                        raise ValueError("Invalid frame type. Please enter 'SFF','EFF' or 'RTR'.")

                    if frame_type == "RTR":
                        can_id = input("Enter CAN ID as hex value: ").strip()
                        can_id = int(can_id, 16)
                        if not (0x0 <= can_id <= 0x1FFFFFFF):
                            raise ValueError("Not a valid CAN ID for an RTR frame.")
                        formatted_id = f"0x{can_id:08X}"
                        json_data.update(
                            {
                                "frame_format": frame_type,
                                "can_id": formatted_id,
                                "bitrate": bitrate,
                            })
                        break

                    else:
                        can_id = input("Enter CAN ID as hex value: ").strip()
                        can_id = int(can_id, 16)
                        if(frame_type == "SFF"):
                            if not (0x0 <= can_id <= 0x7FF):
                                raise ValueError("Not a valid Standard format frame CAN ID.")
                            formatted_id = f"0x{can_id:03X}"
                            break

                        if(frame_type == "EFF"):
                            if not (0x0 <= can_id <= 0x1FFFFFFF):
                                raise ValueError("Not a valid Extended format frame CAN ID.")
                            formatted_id = f"0x{can_id:08X}"
                            break
                except ValueError as e:
                    print(f"Error: {e}")


            if frame_type != "RTR":
                while True:
                    try:
                        dlc = int(input("Enter CAN DLC (Data Length Code): ").strip())
                        if not (0 <= dlc <= 8):
                            raise ValueError("Not a valid CAN DLC.")
                        break
                    except ValueError as e:
                        print(f"Error: {e}")

                while True:
                    payload = input(
                        "Input payload hex bytes, separated by spaces e.g. 11 22 33: ").strip()
                    can_id_bytes = payload.split()
                    if len(can_id_bytes) != dlc:
                        print(f"Error: Payload length does not match DLC. Expected {dlc} bytes, got {len(can_id_bytes)} bytes.")
                        continue

                    try:
                        can_id_hex = [int(b, 16) for b in can_id_bytes]
                        formatted_can_id = [f"0x{byte:02X}" for byte in can_id_hex]
                        formatted_payload = "[" + ",".join(formatted_can_id) + "]"
                    except ValueError:
                        print("Error: All bytes must be valid hexadecimal values.")
                        continue
                    break

                cycle = input("Add frame cycle time? Y/N: ").strip().upper()
                if cycle == "Y":
                    cycle_time = int(input("Enter cycle time in ms: ").strip())
                    json_data.update({"cycle_ms": cycle_time})

                json_data.update(
                    {
                        "frame_format": frame_type,
                        "can_id": formatted_id,
                        "bitrate": bitrate,
                        "dlc": dlc,
                        "payload": formatted_payload,
                    }
                )

        except ValueError as e:
            print(f"Error: {e}")
            return None

    elif command == "read":
        try:
            if write_dict.__len__() > 0:
                if method_check(command):
                    repeat = input("Repeat last read request? (Y/N): ").strip().upper()
                    if repeat == "Y":
                        res_r = repeat_request(command)
                        json_send = f'R"({res_r})"'
                        json_send += "\n"
                        return json_send

            pcan_bitrates_str = "\n ".join(
                [f"{rate} bps ({add.PCAN_BITRATES[rate]})" for rate in add.PCAN_BITRATES]
            )
            print(f"Supported bitrates:\n {pcan_bitrates_str}")

            while True:
                try:
                    bitrate = int(input("Enter desired bitrate: ").strip())
                    if bitrate not in add.PCAN_BITRATES:
                        raise ValueError(
                            f"Unsupported bitrate. Choose one of the supported bitrates."
                        )
                    break
                except ValueError as e:
                    print(f"Error: {e}")

            mask = input("Enable filtering of CAN frames? Y/N: ").strip().upper()
            if mask == "Y":
                while True:
                    try:
                        can_ids = input("Input CAN ID hex bytes, separated by spaces e.g. 11 22 33: ").strip()
                        can_id_bytes = can_ids.split()
                        for id in can_id_bytes:
                            if not (0 <= int(id) <= 2047):
                                raise ValueError("There is an unvalid CAN ID.")
                            break
                    except ValueError as e:
                        print(f"Error: {e}")
                        continue

                    try:
                        can_id_hex = [int(b, 16) for b in can_id_bytes]
                        formatted_can_ids = [f"0x{byte:02X}" for byte in can_id_hex]
                        formatted_can_id = "[" + ",".join(formatted_can_ids) + "]"
                    except ValueError:
                        print("Error: All bytes must be valid hexadecimal values.")
                        continue
                    break

                while True:
                    try:
                        can_masks = input(
                            "Input CAN mask hex bytes, separated by spaces e.g. 11 22 33: ").strip()
                        can_mask_bytes = can_masks.split()
                        if len(can_mask_bytes) != len(can_id_bytes):
                            raise ValueError("Insufficient number of CAN masks.")
                        for id in can_mask_bytes:
                            if not (0 <= int(id) <= 2047):
                                raise ValueError("There is an unvalid CAN mask.")
                            break
                    except ValueError as e:
                        print(f"Error: {e}")
                        continue

                    try:
                        can_mask_hex = [int(b, 16) for b in can_mask_bytes]
                        formatted_can_masks = [f"0x{byte:02X}" for byte in can_mask_hex]
                        formatted_can_mask = "[" + ",".join(formatted_can_masks) + "]"
                    except ValueError:
                        print("Error: All bytes must be valid hexadecimal values.")
                        continue
                    break

                json_data.update(
                    {
                        "bitrate": bitrate,
                        "filter_enable" : "YES",
                        "can_id": formatted_can_id,
                        "can_mask": formatted_can_mask,
                    }
                )

            else:
                json_data.update({"bitrate": bitrate, "filter_enable" : "NO"})

        except ValueError as e:
            print(f"Error: {e}")
            return None

    j = json.dumps(json_data)
    check_w_count(j)
    dp.print_input(json_data["method"], j)
    json_send = f'R"({j})"'
    json_send += "\n"
    return json_send
