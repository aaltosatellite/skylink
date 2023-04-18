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

import skylink
s = "66 4f 48 32 41 53 33 2a 05 04 23 54 00 fa 00 f7 ab 21 62 52 23 6d d6 be a0 b2"     # Skymodem received packet
print(skylink.parse(bytes.fromhex(s)))


