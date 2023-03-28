# Investigation on what the expected Skylink JSON format is.
# This file is reversed-engineered from skylink/skymodem/vc_interface.cpp (27.3.2023)
# All data is relevant as of the "investigation" date


# Sendable template Skymodem JSON frames
# This is also how data is passed through to host application (like porthouse or test script)
TEMPLATE_DATA_FRAME = {
    "packet_type": "uplink",                    # cannot be "downlink" because we are SENDING this to skymodem
    "timestamp": "2023-03-27T12:31:13+00:00",   # REQ: Must be ISO 8601 timestamp
    "vc": 1,                                    # REQ: value must be between 0 and 3 (To what VC we want to send)
    "data": "123456789abcdef"                   # REQ: data length must be of even length
} 

TEMPLATE_CONTROL_FRAME = {
    "metadata": {
        "cmd": "get_state" | "flush" | "get_stats" | "clear_stats" | "set_config" | "get_config" | "arq_connect" | "arq_connect" | "mac_reset",
        "config": "?",
        "value": "?"
    }
}

TEMPLATE_RESPONSE_FRAME = {
    "type": "control",
    "timestamp": "2023-03-27T12:31:13+00:00",
    "vc": 1,                                    # This VC data is not always sent back to publisher
    "metadata": {
        "rsp": "arq_timeout" | "arq_connected",
        "vc": 1,                                # For example error code for what VC
    }
}


