"""
Prints out bytes for a skylink encoded AX.25 repeater frame.
"""
from repeater import encode_ax25_address, ax25_crc16

from skylink_wrapper.cython_skylink import SkyLink, SkyConfiguration
skylink_config_ = SkyConfiguration(identity=b"PyGS")
skylink_config_.vc[3].require_authentication = 0 # No authentication for repeater frame
skylink = SkyLink(skylink_config_)


if __name__ == "__main__":

    message = input("Input message for repeater frame (max 111 bytes) Default: Hello World! : ")
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

    print("Frame before skylink:", frame.hex())

    skylink.sky_vc_push_packet_to_send(3, bytes(frame))
    tx_i, frame_bytes = skylink.sky_tx_with_golay()
    print("Frame bytes with Golay, Reed-solomon, whitening and skylink framing:\n", frame_bytes.hex())
    # Ask if syncword should be added
    add_syncword = input("Add syncword to frame bytes? (y/n, default y): ")
    if add_syncword.strip().lower() in ["", "y", "yes"]:
        syncword = b"\x1A\xCF\xFC\x1D"
        frame_bytes = syncword + frame_bytes
        # Ask if preamble should be added:
        add_preamble = input("Add preamble to frame bytes? (y/n, default y): ")
        if add_preamble.strip().lower() in ["", "y", "yes"]:
            preamble = b"\xAA" * 16
            frame_bytes = preamble + frame_bytes

    print("Final frame bytes:", frame_bytes.hex())