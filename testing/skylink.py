
import struct
from collections import namedtuple

SkyFrame = namedtuple("SkyFrame", ["data", ])

SkyStatistics = namedtuple("SkyStatistics",
    ["rx_frames", "rx_fec_ok", "rx_fec_fail", "rx_fec_octs", "rx_fec_errs", "tx_frames"])

def parse_stats(bs: bytes) -> SkyStatistics:
    """ Parse Skylink statistics structure """
    return SkyStatistics(*struct.unpack(">HHHHHH", bs))


SkyStatus = namedtuple("SkyStatus", ["state", "rx_free", "tx_available"])
def parse_status(bs: bytes) -> SkyStatus:
    """ Parse Skylink statistics structure """
    return SkyStatus(
        struct.unpack(">4B", bs[0:4]),
        struct.unpack(">4H", bs[4:4+8]),
        struct.unpack(">4H", bs[4+8:]),
    )
