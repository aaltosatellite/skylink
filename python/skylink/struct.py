from construct import *

ExtType = Enum(BitsInteger(4),
    ARQSequence=0,
    ARQRequest=1,
    ARQControl=2,
    ARQHandshake=3,
    TDDControl=4,
    HMACSequenceReset=5
)

ExtARQSeq = BitStruct(
    'sequence' / BitsInteger(8)
)

ExtARQReq = BitStruct(
    'sequence' / BitsInteger(8),
    'mask' / BitsInteger(16)
)

ExtARQCtrl = BitStruct(
    'tx_sequence' / BitsInteger(8),
    'rx_sequence' / BitsInteger(8)
)

ExtARQHandshake = BitStruct(
    'peer_state' / BitsInteger(8),
    'identifier' / BitsInteger(32)
)

ExtTDDControl = BitStruct(
    'window' / BitsInteger(16),
    'remaining' / BitsInteger(16)
)

ExtHMACSequenceReset = BitStruct(
    'sequence' / BitsInteger(16)
)

SkyExtensionHeader = BitStruct(
    '_len' / BitsInteger(4),
    'type' / ExtType,
    'data' / Bytewise(Switch(this.type,
        {
            ExtType.ARQSequence: ExtARQSeq,
            ExtType.ARQRequest: ExtARQReq,
            ExtType.ARQControl: ExtARQCtrl,
            ExtType.ARQHandshake: ExtARQHandshake,
            ExtType.TDDControl: ExtTDDControl,
            ExtType.HMACSequenceReset: ExtHMACSequenceReset
        }, default=Bytes(this._len)
    ))
)


def ARQSequence(sequence: int) -> bytes:
    return SkyExtensionHeader.build({
        '_len': 1,
        'type': ExtType.ARQSequence,
        'data' : {
            'sequence': sequence
        }
    })


def ARQRequest(sequence: int, mask: int) -> bytes:
    return SkyExtensionHeader.build({
        '_len': 2,
        'type': ExtType.ARQRequest,
        'data' : {
            'sequence': sequence,
            'mask': mask
        }
    })

def ARQControl(tx_sequence: int, rx_sequence: int) -> bytes:
    return SkyExtensionHeader.build({
        '_len': 4,
        'type': ExtType.ARQControl,
        'data' : {
            'tx_sequence': tx_sequence,
            'rx_sequence': rx_sequence
        }
    })

def ARQHandshake(peer_state: int, identifier: int) -> bytes:
    return SkyExtensionHeader.build({
        '_len': 5,
        'type': ExtType.ARQHandshake,
        'data' : {
            'peer_state': peer_state,
            'identifier': identifier
        }
    })

def TDDControl(window: int, remaining: int) -> bytes:
    return SkyExtensionHeader.build({
        '_len': 4,
        'type': ExtType.TDDControl,
        'data' : {
            'window': window,
            'remaining': remaining
        }
    })

def HMACSequenceReset(sequence: int) -> bytes:
    return SkyExtensionHeader.build({
        '_len': 2,
        'type': ExtType.HMACSequenceReset,
        'data' : {
            'sequence': sequence
        }
    })


SkyHeaderFlags = Struct(
    'fragment' / BitsInteger(2),
    'has_payload' / Flag,
    'arq_on' / Flag,
    'is_authenticated' / Flag,
)

SkyHeader = BitStruct(
    'version' / BitsInteger(5),
    '_ident_len' / BitsInteger(3),
    'identifier' / Bytewise(Bytes(this._ident_len)),
    'flags' / SkyHeaderFlags,
    'vc' / BitsInteger(3),
    'extension_len' / BitsInteger(8),
    'sequence' / BitsInteger(16),
    'extensions' / IfThenElse(
        lambda ctx: ctx._root._.parse_extensions,
        RestreamData(Bytewise(Bytes(this.extension_len)), GreedyRange(SkyExtensionHeader)),
        Bytewise(Bytes(this.extension_len))
    )
)

def _payload_len(ctx: Construct) -> int:
    """ Resolve payload field length. """
    return ctx._.frame_len - 5 - ctx.header._ident_len - ctx.header.extension_len - (8 if ctx.header.flags.is_authenticated == 1 else 0)


SkyRadioFrame = Struct(
    'header' / SkyHeader,
    'payload' / If(this.header.flags.has_payload, Bytes(_payload_len)),
    'auth' / If(this.header.flags.is_authenticated == 1, Bytes(8)),
)

def parse_struct(frame: bytes, parse_extensions: bool=False) -> SkyRadioFrame:
    return SkyRadioFrame.parse(frame, parse_extensions=parse_extensions, frame_len=len(frame))
