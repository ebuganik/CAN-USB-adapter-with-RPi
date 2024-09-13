import display as dp
import json
import time

# Dictionaries to save info about written and read frames (+ cycles count)

write_dict = {}
read_dict = {}
read_dict_cycle = {}

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


def method_check(method):
    for key in reversed(write_dict):
        if method in key:
            return True
    return False


def repeat_request(method):
    # Returns last found key by method
    for key in reversed(write_dict):
        if method in key:
            check_w_count(key)
            return key


# Function to check write/read requests
def check_w_count(write_string):
    # Check if it's already in dictionary
    if write_string in write_dict:
        # Increment the count for the existing key
        write_dict[write_string] += 1
    else:
        # Add new key with count 1
        write_dict[write_string] = 1
        print(write_dict)


# Function to check read frames from serial, to track time interval between same frames and count them
def check_r_count(read_string):
    current_time = time.time()
    if read_string in read_dict:
        # Increment the count
        read_dict[read_string] += 1
        # Calculate the time interval since the last occurrence and update the dictionary
        time_interval = (current_time - read_dict_cycle[read_string]) * 1000
        read_dict_cycle[read_string] = current_time
        return time_interval
    else:
        # Initialize the count and timestamp for the new frame
        read_dict[read_string] = 1
        read_dict_cycle[read_string] = current_time
        return 0


def read_input():
    while True:
        method = (
            input("Enter method (write or read or CTRL+C to exit): ").strip().lower()
        )
        if method in ["write", "read"]:
            break
        else:
            print("Method unknown. Enter 'write' or 'read'.")

    json_data = {"method": method}

    # To send write request via serial, JSON string should be packed as 'R"({"method":write","bitrate":250000,"id":"0x123","dlc":4,"payload":"[0x11,0x22,0x33,0x44]"})"'
    if method == "write":
        try:

            # Add check in case user wants to repeat last write request
            if method_check(method):
                if write_dict.__len__() > 0:
                    repeat = input("Repeat last write request? (Y/N): ").strip()
                    if repeat == "Y":
                        res_w = repeat_request(method)
                        json_send = f'R"({res_w})"'
                        json_send += "\n"
                        return json_send
            # Print PCAN-USB supported bitrates to choose
            pcan_bitrates_str = ", ".join(
                [f"{rate} bps ({PCAN_BITRATES[rate]})" for rate in PCAN_BITRATES]
            )
            print(f"Supported bitrates: {pcan_bitrates_str}")

            while True:
                try:
                    bitrate = int(input("Enter desired bitrate: ").strip())
                    if bitrate not in PCAN_BITRATES:
                        raise ValueError(
                            f"Unsupported bitrate. Choose one of the supported bitrates."
                        )
                    break
                except ValueError as e:
                    print(f"Error: {e}")

            # CAN ID input
            while True:
                try:
                    can_id = input("Enter CAN ID as hex value: ").strip()
                    can_id = int(can_id, 16)
                    if not (0 <= can_id <= 2047):  # Check Standard CAN ID range
                        raise ValueError("Not a valid CAN ID.")
                    formatted_id = f"0x{can_id:03X}"
                    break  # input valid
                except ValueError as e:
                    print(f"Error: {e}")

            # DLC
            while True:
                try:
                    dlc = int(input("Enter CAN DLC (Data Length Code): ").strip())
                    if not (0 <= dlc <= 8):  # DLC range
                        raise ValueError("Not a valid CAN DLC.")
                    break
                except ValueError as e:
                    print(f"Error: {e}")

            # Payload input check
            while True:
                payload = input(
                    "Input payload hex bytes, separated by spaces e.g. 11 22 33: "
                ).strip()
                can_id_bytes = payload.split()
                if len(can_id_bytes) != dlc:
                    print(
                        f"Error: Payload length does not match DLC. Expected {dlc} bytes, got {len(can_id_bytes)} bytes."
                    )
                    continue

                # Format payload bytes
                try:
                    can_id_hex = [int(b, 16) for b in can_id_bytes]
                    formatted_can_id = [f"0x{byte:02X}" for byte in can_id_hex]
                    formatted_payload = "[" + ",".join(formatted_can_id) + "]"
                except ValueError:
                    print("Error: All bytes must be valid hexadecimal values.")
                    continue
                break
            while True:
                cycle = input("Add frame cycle time? Y/N: ").strip()
                if cycle in ["Y", "N"]:
                    break
                else:
                    print("Invalid input. Enter 'Y' or 'N'.")
                    
            if cycle == "Y":
                cycle_time = int(input("Enter cycle time in ms, e.g between 1 and 10: ").strip())
                json_data.update({"cycle_ms": cycle_time})
                      
                # Pack data into JSON
            json_data.update(
                {
                    "can_id": formatted_id,
                    "bitrate": bitrate,
                    "dlc": dlc,
                    "payload": formatted_payload,
                }
            )

        except ValueError as e:
            print(f"Error: {e}")
            return None

    # To send read request via serial, JSON string should be packed as 'R"({"method":read","bitrate":250000})"'
    # In case filtering CAN frames should be enabled, JSON string should be packed as 'R"({"method":read","bitrate":250000, "can_id": "[0x123, 0x200]", "can_mask": "[0x7FF, 0x700]"})"'
    elif method == "read":
        try:
            # Add check in case user wants to repeat last read request
            if method_check(method):
                if write_dict.__len__() > 0:
                    repeat = input("Repeat last read request? (Y/N): ").strip()
                    if repeat == "Y":
                        res_r = repeat_request(method)
                        json_send = f'R"({res_r})"'
                        json_send += "\n"
                        return json_send

            # Print PCAN-USB supported bitrates to choose
            pcan_bitrates_str = ", ".join(
                [f"{rate} bps ({PCAN_BITRATES[rate]})" for rate in PCAN_BITRATES]
            )
            print(f"Supported bitrates: {pcan_bitrates_str}")

            while True:
                try:
                    bitrate = int(input("Enter desired bitrate: ").strip())
                    if bitrate not in PCAN_BITRATES:
                        raise ValueError(
                            f"Unsupported bitrate. Choose one of the supported bitrates."
                        )
                    break
                except ValueError as e:
                    print(f"Error: {e}")

            # Check if user wants enabled filtering
            mask = input("Enable filtering of CAN frames? Y/N: ").strip()
            if mask == "Y":
                # Enter CAN ID
                while True:
                    try:
                        can_ids = input(
                            "Input CAN ID hex bytes, separated by spaces e.g. 11 22 33: "
                        ).strip()
                        can_id_bytes = can_ids.split()
                        for id in can_id_bytes:
                            if not (0 <= int(id) <= 2047):
                                raise ValueError("There is an unvalid CAN ID.")
                            break
                    except ValueError as e:
                        print(f"Error: {e}")
                        continue

                    # Format CAN ID bytes
                    try:
                        can_id_hex = [int(b, 16) for b in can_id_bytes]
                        formatted_can_ids = [f"0x{byte:02X}" for byte in can_id_hex]
                        formatted_can_id = "[" + ",".join(formatted_can_ids) + "]"
                    except ValueError:
                        print("Error: All bytes must be valid hexadecimal values.")
                        continue
                    break

                while True:
                    # Enter CAN masks
                    try:
                        can_masks = input(
                            "Input CAN mask hex bytes, separated by spaces e.g. 11 22 33: "
                        ).strip()
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

                    # Format CAN ID bytes
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
                        "can_id": formatted_can_id,
                        "can_mask": formatted_can_mask,
                    }
                )

            # No filtering option
            else:
                # Pack data into JSON
                json_data.update({"bitrate": bitrate})

        except ValueError as e:
            print(f"Error: {e}")
            return None

    # Adds new JSON object to list of written JSON objects with count field
    j = json.dumps(json_data)
    check_w_count(j)
    dp.print_input(json_data["method"], j)
    json_send = f'R"({j})"'
    json_send += "\n"
    return json_send
