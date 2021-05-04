
import struct
from collections import namedtuple

SkyFrame = namedtuple("SkyFrame", ["data",])

SkyStatistics = namedtuple("SkyStatistics", ["a", "b", "c"])
def parse_stats(bs: bytes) -> SkyStatistics:
    """ Parse Skylink statistics structure """
    return SkyStatistics(*struct.unpack(">UUU", bs))


SkyStatus = namedtuple("SkyStatus", ["a", "b", "c"])
def parse_status(bs: bytes) -> SkyStatus:
    """ Parse Skylink statistics structure """
    return SkyStatus(*struct.unpack(">UUU", bs))
