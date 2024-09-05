import json

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


def read_input():
    while True:
        method = input("Enter method (write or read): ").strip().lower()
        if method in ["write", "read"]:
            break
        else:
            print("Method unknown. Enter 'write' or 'read'.")

    json_data = {"method": method}

    # To send write request via serial, JSON string should be packed as 'R"({"method":write","bitrate":250000,"id":"0x123","dlc":4,"payload":"[0x11,0x22,0x33,0x44]"})"'
    if method == "write":
        try:
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

    # To send read request via serial, JSON string should be packed as 'R"({"method":write","bitrate":250000})"'
    # In case filtering CAN frames should be enabled, JSON string should be packed as 'R"({"method":write","bitrate":250000, "can_id": "[0x123, 0x200]", "can_mask": "[0x7FF, 0x700]"})"'
    elif method == "read":
        try:
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
        
    # JSON object to JSON formatted string
    j = json.dumps(json_data)
    json_send = f'R"({j})"'
    json_send += "\n"
    return json_send
