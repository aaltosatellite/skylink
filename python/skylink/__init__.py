"""
"""
from .golay24 import encode_golay24_bytes, decode_golay24_bytes
from .fec import SkylinkError, randomize, decode_rs, encode_rs
from .auth import hmac_verify, hmac_append
from .struct import SkyHeader, SkyRadioFrame, parse_struct

from typing import Optional, Union


def parse_frame(
        raw: bytes,
        includes_golay: bool=False,
        includes_fec: bool=False,
        hmac_key: Optional[bytes]=None,
        parse_extensions: bool=False
    ) -> SkyRadioFrame:
    """
    Parse bytes to SkyRadioFrame object.

    Args:
        raw: Raw bytes
        golay: If True, the Golay24 header is included to the beginning.
        fec: If True, FEC trailer and randomizers are included in the frame.
        hmac_key: HMAC key as bytes.
            If key is provided and the frame is signed, the authentication code will be verified.
        parse_extensions: Parse also extension header

    Raises:
        `GolayUncorrectable` in case the Golay code fails
        `FECError` in case FEC fails to decode the frame.
        `HMACError` in case HMAC verification fails.

    Returns:
        A `SkyRadioFrame` object
    """

    if includes_golay:
        decoded = decode_golay24_bytes(raw)
        decoded_len = 0xFF & decoded
        if decoded_len + 3 < len(raw):
            raise SkylinkError("Golay Error: Frame is too short")
        raw = raw[3+decoded_len:]

        if (decoded & 0x200) != 0: # RS Enabled
            includes_fec = True
        if (decoded & 0x400) != 0: # Randomizer enabled
            randomizer = True
        if (decoded & 0x800) != 0: # Viterbi enabled
            raise SkylinkError("Viterbi not supported")

    if includes_fec:
        raw = randomize(raw)
        raw, bytes_corrected = decode_rs(raw)

    # frame = SkyRadioFrame.parse(frame, parse_extensions=parse_extensions, frame_len=len(frame))
    frame = parse_struct(raw, parse_extensions=parse_extensions)
    if hmac_key is not None and frame.header.flags.is_authenticated:
        hmac_verify(raw, hmac_key)

    return frame



def construct_frame(
        identity: bytes,
        sequence: int,
        vc: int,
        payload: bytes,
        fragment: int=0,
        extensions: Union[bytes, None]=b'',
        append_golay: bool=False,
        append_fec: bool=False,
        hmac_key: Optional[bytes]=None
    ) -> bytes:
    """
    Construct a Skylink frame (bytes) from given information.

    Args:
        identity:
        sequence: Frame sequence number as integer
        vc: Virtual channel index as integer (0-3)
        payload: Frame payload as bytes
        fragment: Frament
        extensions: Header extensions
        append_golay: If true, Golay coded length is added to the beginning of frame.
        append_fec: If true, Reed-Solomon FEC as appended to the frame.
        hmac_key: Authentication key as bytes.

    Returns:
        A 'bytes' string.
    """

    if append_golay == True and append_fec == False:
        raise ValueError("Invalid config")

    #if isinstance(extensions, todo):

    header: bytes = SkyHeader.build({
        "version": 12,
        "_ident_len": len(identity),
        "identifier": identity,
        "flags": {
            "fragment": fragment,
            "has_payload": 1 if len(payload) > 0 else 0,
            "arq_on": 0,
            "is_authenticated": 1 if hmac_key is not None else 0,
        },
        "vc": vc,
        "extension_len": len(extensions),
        "sequence": sequence,
        "extensions": extensions
    }, parse_extensions=False)

    raw = header + payload
    if hmac_key is not None:
        raw = hmac_append(raw, hmac_key)
    if append_fec:
        raw = randomize(encode_rs(raw))
    if append_golay:
        raw = encode_golay24_bytes(0x0000 + len(raw)) + raw

    return raw
