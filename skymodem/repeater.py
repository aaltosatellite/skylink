"""
Connect to the SkyModem Radio Amateur Virtual Channel and construct and send frames to be repeated by the satellite.

This is meant to run seperately alongside skymodem application which handles the actual modem operation.

In other words this script will only construct the AX.25 frame and send it to SkyLink VC3 in the running modem instance.
"""


crc16_table = [
	0x0000, 0x1081, 0x2102, 0x3183,
	0x4204, 0x5285, 0x6306, 0x7387,
	0x8408, 0x9489, 0xa50a, 0xb58b,
	0xc60c, 0xd68d, 0xe70e, 0xf78f
    ]

def ax25_crc16(data):
    """
    Calculate AX.25 CRC16 for given data.
    
    Args:
        data: bytes or bytearray
        
    Returns:
        16-bit CRC value
    """
    crc = 0xFFFF
    for byte in data:
        crc = (crc >> 4) ^ crc16_table[(crc & 0xf) ^ (byte & 0xf)]
        crc = (crc >> 4) ^ crc16_table[(crc & 0xf) ^ (byte >> 4)]
    return (~crc) & 0xFFFF  # Mask to 16 bits since Python ints are arbitrary precision

def ax25_shift1(data):
    """
    Callsigns are shifted left by 1 bit in AX.25 frames.
    """

    shifted = bytearray()
    for byte in data:
        shifted.append((byte << 1) & 0xFF)  # Shift left and mask to 8 bits
    return shifted


def encode_ax25_address(callsign, ssid=0, is_last=False):
    """
    Encode an AX.25 address field (callsign + SSID byte).
    
    Args:
        callsign: Callsign string (max 6 chars)
        ssid: SSID value (0-15)
        is_last: True if this is the last address in the address field
        
    Returns:
        7 bytes: 6 bytes shifted callsign + 1 byte SSID
    """
    # Pad callsign to 6 characters with spaces
    callsign_padded = callsign.upper().ljust(6)[:6]
    
    # Shift callsign left by 1
    shifted_callsign = ax25_shift1(callsign_padded.encode('ascii'))
    
    # Construct SSID byte: bits 1-4 are SSID, bit 5-6 are reserved (11), bit 7 is extension bit
    # Before shifting: 0bCRRSSSSE where C=command/response, RR=reserved(11), SSSS=SSID, E=extension
    ssid_byte = 0b01100000 | ((ssid & 0x0F) << 1) | (1 if is_last else 0)
    
    return shifted_callsign + bytes([ssid_byte])

def parse_frame(frame):
    """
    Parse an AX.25 frame and return its components.
    
    Args:
        frame: bytearray of the AX.25 frame
    Returns:
        dict with keys: dest_addr, src_addr, digipeaters (list), control_byte, pid, info, crc
    """

    addresses = []
    index = 0
    while True:
        addr_bytes = frame[index:index+7]
        callsign = bytearray()
        for b in addr_bytes[:6]:
            callsign.append(b >> 1)
        callsign_str = callsign.decode('ascii').rstrip()
        ssid = (addr_bytes[6] >> 1) & 0x0F
        addresses.append((callsign_str, ssid))
        index += 7
        if addr_bytes[6] & 0x01:
            break

    dest_addr, src_addr = addresses[0], addresses[1]
    digipeaters = addresses[2:] if len(addresses) > 2 else []

    control_byte = hex(frame[index])
    pid = hex(frame[index + 1])
    info = frame[index + 2:-2]
    crc = hex((frame[-2] << 8) | frame[-1])

    return {
        "dest_addr": dest_addr,
        "src_addr": src_addr,
        "digipeaters": digipeaters,
        "control_byte": control_byte,
        "pid": pid,
        "info": info,
        "crc": crc
    }




if __name__ == "__main__":
    import os
    import zmq
    
    vc_base = input("Input skymodem virtual channel base (default 7100): ")
    if vc_base.strip() == "":
        vc_base = 7100
    else:
        vc_base = int(vc_base)
    
    context = zmq.Context()
    sub_sock = context.socket(zmq.SUB)
    sub_sock.connect(f"tcp://localhost:{vc_base + 3*10}")
    sub_sock.subscribe(b"")
    sub_sock.set(zmq.RCVTIMEO, 1000)

    # Publish to send data TO skymodem, which will send it to the satellite to be repeated.
    pub_sock = context.socket(zmq.PUB)
    pub_sock.connect(f"tcp://localhost:{vc_base + 3*10 + 1}")
    print("Connected to SkyModem VC repeater interface.")

    message = input("Input message to repeat via repeater (max 111 bytes) Default: Hello World! : ")
    if message.strip() == "":
        message = "Hello World!"
    if len(message) > 111:
        print("Message too long, truncating to 111 bytes.")
        message = message[:111]
    dest_addr = input("Input destination callsign (default OH2F1S): ")
    if dest_addr.strip() == "":
        dest_addr = "OH2F1S"
    src_addr = input("Input source callsign (default OH2AGS): ")
    if src_addr.strip() == "":
        src_addr = "OH2AGS"

    # Input digipeater addresses
    digis = []
    while True:
        digi = input("Input digipeater callsign (or press enter to finish): ")
        if digi.strip() == "":
            break
        digis.append(digi.strip())
        if len(digis) >= 8:
            print("Maximum of 8 digipeaters reached.")
            break

    # Control byte and PID
    control_byte = 0x03  # UI frame
    pid = 0xF0  # No layer 3 protocol

    # Construct frame, calculate CRC and append it to the frame.
    frame = bytearray()

    # Destination address (SSID 0, not last since source follows)
    frame.extend(encode_ax25_address(dest_addr, ssid=0, is_last=False))

    # Source address (SSID 0, last=True if no digipeaters, False otherwise)
    frame.extend(encode_ax25_address(src_addr, ssid=0, is_last=(len(digis) == 0)))

    # Digipeater addresses
    for i, digi in enumerate(digis):
        is_last_digi = (i == len(digis) - 1)
        frame.extend(encode_ax25_address(digi, ssid=0, is_last=is_last_digi))

    # Control byte
    frame.append(control_byte)

    # PID
    frame.append(pid)

    # Information field (message)
    frame.extend(message.encode('ascii'))

    # Calculate CRC
    crc = ax25_crc16(frame)
    # Append CRC (little-endian)
    frame.append((crc >> 8) & 0xFF)
    frame.append(crc & 0xFF)

    # Send frame to skymodem for transmission
    pub_sock.send_json({"data": frame.hex()})
    print(f"Sent frame to skymodem for repeating via satellite. Frame length: {len(frame)} bytes.")

    # Print frame in hex
    print("Payload (hex): ", frame.hex())
    while True:
        try:
            msg = sub_sock.recv_json()
            print("Received repeater frame from satellite: ", msg["data"])
            parsed_frame = parse_frame(bytearray.fromhex(msg["data"]))
            print("Parsed frame: ", parsed_frame)
        except zmq.Again:
            pass