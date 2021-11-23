

from construct import *
from construct import Int8ub, Int16ub, Flag


SkyExtension = Struct(
    'ident' / BitsInteger(4),
    'len' / BitsInteger(4),
    'data' / Bytes(this.len),
)

SkyHeaderFlags = Struct(
    'flag3' / Flag,
    'flag2' / Flag,
    'flag1' / Flag,
    'is_authenticated' / Flag
)

SkyHeader = Struct(
    'version' / BitsInteger(4),
    'type' / BitsInteger(4),
    'identifier' / BitsInteger(5*8),
    'vc' / BitsInteger(4),
    'flags' / SkyHeaderFlags,
    'sequence' / Int16ub,
    'ext_len' / Int8ub,
    'extensions' / Bytes(this.ext_len),
    #'extensions' / RestreamData(Bytes(this.ext_len), GreedyRange(SkyExtension))
)

def _payload_len(ctx: Construct) -> int:
    """ Resolve payload field length. """
    return ctx._.frame_len - len(this.header) - (8 if ctx.header.flags.is_authenticated else 0)

SkyRadioFrame = Struct(
    'header' / SkyHeader,
    'payload' / Bytes(_payload_len),
    'auth' / If(this.header.flags.is_authenticated == 1, Bytes(8))
)


def parse(frame: bytes) -> SkyRadioFrame:
    return SkyRadioFrame.parse(frame, frame_len=len(frame))
