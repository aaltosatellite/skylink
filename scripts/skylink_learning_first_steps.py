# Investigation on what the expected Skylink JSON format is.
# This file is reversed-engineered from skylink/skymodem/vc_interface.cpp (27.3.2023)
# All data is relevant as of the "investigation" date


# Sendable template Skymodem JSON frames
# This is also how data is passed through to host application (like porthouse or test script)
TEMPLATE_DATA_FRAME = {
    "packet_type": "uplink",                    # OPT - cannot be "downlink" because we are SENDING this to skymodem
    "timestamp": "2023-03-27T12:31:13+00:00",   # OPT - REQ: Must be ISO 8601 timestamp
    "vc": 1,                                    # OPT - REQ: value must be between 0 and 3 (To what VC we want to send)
    "data": "123456789abcdef"                   # REQ: data length must be of even length
} 

TEMPLATE_CONTROL_FRAME = {
    "metadata": {
        "cmd": "get_state" | "flush" | "get_stats" | "clear_stats" | "set_config" | "get_config" | "arq_connect" | "arq_disconnect" | "mac_reset",
        "config": "?",
        "value": "?"
    }
}

TEMPLATE_RESPONSE_FRAME = {
    "type": "control",
    "timestamp": "2023-03-27T12:31:13+00:00",
    "vc": 1,                                    # This VC data is not always sent back to publisher
    "metadata": {
        "rsp": "state" | "stats" | "config" | "arq_timeout" | "arq_connected",
        "vc": 1,                                # For example error code for what VC
    }
}

# Manually parsing packets as received by suo terminal 
import skylink
s = "66 4f 48 32 41 53 33 2a 05 04 23 54 00 fa 00 f7 ab 21 62 52 23 6d d6 be a0 b2"     # Skymodem received packet
print(skylink.parse(bytes.fromhex(s)))

# Parse SkylinkConfig
class SkyConfig(NamedTuple):
    # SkyPHYConfig:
    enable_scrambler: int
    enable_rs: int

    # SkyMACConfig:
    maximum_window_length_ticks: int
    minimum_window_length_ticks: int
    gap_constant_ticks: int
    tail_constant_ticks: int
    shift_threshold_ticks: int
    idle_timeout_ticks: int
    window_adjust_increment_ticks: int
    carrier_sense_ticks: int
    unauthenticated_mac_updates: int
    window_adjustment_period: int
    idle_frames_per_window: int

    # SkyHMACConfig:
    hmac_key_length: int
    hmac_maximum_jump: int
    hmac_key: bytes

    # SkyVCConfig:
    vc0_element_size: int
    vc0_rcv_ring_len: int
    vc0_horizon_width: int
    vc0_send_ring_len: int
    vc0_require_authentication: int

    # SkyVCConfig:
    vc1_element_size: int
    vc1_rcv_ring_len: int
    vc1_horizon_width: int
    vc1_send_ring_len: int
    vc1_require_authentication: int

    # SkyVCConfig:
    vc2_element_size: int
    vc2_rcv_ring_len: int
    vc2_horizon_width: int
    vc2_send_ring_len: int
    vc2_require_authentication: int

    # SkyVCConfig:
    vc3_element_size: int
    vc3_rcv_ring_len: int
    vc3_horizon_width: int
    vc3_send_ring_len: int
    vc3_require_authentication: int

    # Identity
    identity: bytes

    # SkyARQConfig:
    arq_timeout_ticks: int
    arq_idle_frame_threshold: int
    arq_idle_frames_per_window: int

data = lambda x: SkyConfig(*struct.unpack("<2Bxx" +    # PHY config
                                   "6i2hBbBx" + # TDD MAC config
                                   "ii32s" +    # HMAC config
                                   4*"4iBxxx" + # Virtual channel config
                                   "6sxx" +     # ID
                                   "2iBxxx",    # ARQ config
                                   x))
