
from enum import Enum
import struct
from typing import List, NamedTuple

#SkyFrame = namedtuple("SkyFrame", ["data", ])

class SkyStatistics(NamedTuple):
    rx_frames: int
    rx_fec_ok: int
    rx_fec_fail: int
    rx_fec_octs: int
    rx_fec_errs: int
    rx_arq_resets: int
    tx_frames: int
    tx_bytes: int


def parse_stats(bs: bytes) -> SkyStatistics:
    """ Parse Skylink statistics structure """
    return SkyStatistics(*struct.unpack("!8H", bs))


class ARQState(Enum):
    ARQ_STATE_OFF       = 0
    ARQ_STATE_IN_INIT   = 1
    ARQ_STATE_ON        = 2


class SkyVCState(NamedTuple):
    state: ARQState
    buffer_free: int
    tx_frames: int
    rx_frames: int

class SkyState(NamedTuple):
    vc: List[SkyVCState]


def parse_state(bs: bytes) -> SkyState:
    """ Parse Skylink statistics structure """
    return SkyState(
        [ SkyVCState(*struct.unpack("<4H", bs[0:8])),
          SkyVCState(*struct.unpack("<4H", bs[8:16])),
          SkyVCState(*struct.unpack("<4H", bs[16:24])),
          SkyVCState(*struct.unpack("<4H", bs[24:32])) ]
    )
