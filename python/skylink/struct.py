

from construct import *
from construct import Int8ub, Int16ub, Flag


SkyExtensionHeader = BitStruct(
    '_extension_len' / BitsInteger(4),
    'extension_type' / BitsInteger(4),
    'data' / Bytewise(Bytes(this._extension_len - 1)),
)

SkyHeaderFlags = Struct(
    Padding(2),
    'has_payload' / Flag,
    'arq_on' / Flag,
    'is_authenticated' / Flag
)

SkyHeader = BitStruct(
    'version' / BitsInteger(5),
    '_ident_len' / BitsInteger(3),
    'identifier' / Bytewise(Bytes(this._ident_len)),
    'flags' / SkyHeaderFlags,
    'vc' / BitsInteger(3),
    '_extension_len' / BitsInteger(8),
    'sequence' / BitsInteger(16),
    'extensions' / Bytewise(Bytes(this._extension_len)),
)

def _payload_len(ctx: Construct) -> int:
    """ Resolve payload field length. """
    return ctx._.frame_len - 5 - ctx.header._ident_len - ctx.header._extension_len - (8 if ctx.header.flags.is_authenticated == 1 else 0)


SkyRadioFrame = Struct(
    'header' / SkyHeader,
    'payload' / If(this.header.flags.has_payload, Bytes(_payload_len)),
    'auth' / If(this.header.flags.is_authenticated == 1, Bytes(8)),
)


def parse_struct(frame: bytes) -> SkyRadioFrame:
    return SkyRadioFrame.parse(frame, frame_len=len(frame))
