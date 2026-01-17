"""
Prints out bytes for a skylink encoded AX.25 repeater frame.
"""
from __future__ import annotations

import argparse
import sys

from repeater import encode_ax25_address, ax25_crc16

from skylink_wrapper.cython_skylink import SkyLink, SkyConfiguration
skylink_config_ = SkyConfiguration(identity=b"PyGS")
skylink_config_.vc[3].require_authentication = 0 # No authentication for repeater frame
skylink = SkyLink(skylink_config_)


def construct_frame_bytes(
    message: str | None = None,
    dest_addr: str | None = None,
    src_addr: str | None = None,
    digis: list[str] | None = None,
    no_digis: bool = False,
    add_syncword: bool | None = None,
    add_preamble: bool | None = None,
):
    """Construct a SkyLink-encoded AX.25 repeater frame.

    Any argument that is None will be requested interactively.
    """

    if message is None:
        message = input("Input message for repeater frame (max 111 bytes) Default: Hello World! : ")
        if message.strip() == "":
            message = "Hello World!"
    if len(message) > 111:
        print("Message too long, truncating to 111 bytes.")
        message = message[:111]

    if dest_addr is None:
        dest_addr = input("Input destination callsign (default OH2F1S): ")
        if dest_addr.strip() == "":
            dest_addr = "OH2F1S"

    if src_addr is None:
        src_addr = input("Input source callsign (default OH2AGS): ")
        if src_addr.strip() == "":
            src_addr = "OH2AGS"

    # Input digipeater addresses
    if no_digis:
        digis = []
    else:
        if digis is None:
            digis = []
            while True:
                digi = input("Input digipeater callsign (or press enter to finish): ")
                if digi.strip() == "":
                    break
                digis.append(digi.strip())
                if len(digis) >= 8:
                    print("Maximum of 8 digipeaters reached.")
                    break
        else:
            digis = [d.strip() for d in digis if d.strip()]
            if len(digis) > 8:
                print("Maximum of 8 digipeaters reached, truncating.")
                digis = digis[:8]

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
    if add_syncword is None:
        add_syncword_in = input("Add syncword to frame bytes? (y/n, default y): ")
        add_syncword = add_syncword_in.strip().lower() in ["", "y", "yes"]
    if add_syncword:
        syncword = b"\x1A\xCF\xFC\x1D"
        frame_bytes = syncword + frame_bytes
        # Ask if preamble should be added:
        if add_preamble is None:
            add_preamble_in = input("Add preamble to frame bytes? (y/n, default y): ")
            add_preamble = add_preamble_in.strip().lower() in ["", "y", "yes"]
        if add_preamble:
            preamble = b"\xAA" * 16
            frame_bytes = preamble + frame_bytes

    print("Final frame bytes:\n", frame_bytes.hex())

    return frame_bytes


def _parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Construct SkyLink-encoded AX.25 repeater frame bytes")
    parser.add_argument("--message", type=str, default=None, help="Message (max 111 bytes). If omitted, prompt.")
    parser.add_argument("--dest", dest="dest_addr", type=str, default=None, help="Destination callsign. If omitted, prompt.")
    parser.add_argument("--src", dest="src_addr", type=str, default=None, help="Source callsign. If omitted, prompt.")
    parser.add_argument("--digi", dest="digis", action="append", default=None, help="Digipeater callsign, to add multiple use --digi multiple times")
    parser.add_argument("--no-digis", action="store_true", help="Do not include digipeaters and do not prompt for them.")

    sync = parser.add_mutually_exclusive_group()
    sync.add_argument("--syncword", dest="add_syncword", action="store_true", help="Force add syncword")
    sync.add_argument("--no-syncword", dest="add_syncword", action="store_false", help="Force no syncword")
    parser.set_defaults(add_syncword=None)

    pre = parser.add_mutually_exclusive_group()
    pre.add_argument("--preamble", dest="add_preamble", action="store_true", help="Force add preamble (requires syncword)")
    pre.add_argument("--no-preamble", dest="add_preamble", action="store_false", help="Force no preamble")
    parser.set_defaults(add_preamble=None)

    return parser.parse_args(argv)



if __name__ == "__main__":
    args = _parse_args(sys.argv[1:])
    frame_bytes = construct_frame_bytes(
        message=args.message,
        dest_addr=args.dest_addr,
        src_addr=args.src_addr,
        digis=args.digis,
        no_digis=bool(args.no_digis),
        add_syncword=args.add_syncword,
        add_preamble=args.add_preamble,
        )