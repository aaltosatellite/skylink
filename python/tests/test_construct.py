import pytest

from skylink import construct_frame, parse_frame, SkylinkError
from skylink.auth import HMACError
from skylink.struct import SkyExtensionHeader, ExtType, ARQSequence, ARQRequest, ARQControl, ARQHandshake, TDDControl, HMACSequenceReset

from utils import corrupt


def test_arq_sequence():
    data = ARQSequence(sequence=123)
    assert len(data) == 2

    parsed = SkyExtensionHeader.parse(data)
    assert parsed.type == ExtType.ARQSequence
    assert parsed.data.sequence == 123


def test_arq_request():
    data = ARQRequest(sequence=156, mask=0b1101101)
    assert len(data) == 4

    parsed = SkyExtensionHeader.parse(data)
    assert parsed.type == ExtType.ARQRequest
    assert parsed.data.sequence == 156
    assert parsed.data.mask == 0b1101101


def test_arq_control():
    data = ARQControl(tx_sequence=123, rx_sequence=231)
    assert len(data) == 3

    parsed = SkyExtensionHeader.parse(data)
    assert parsed.type == ExtType.ARQControl
    assert parsed.data.tx_sequence == 123
    assert parsed.data.rx_sequence == 231


def test_arq_handshake():
    data = ARQHandshake(peer_state=2, identifier=0xDEADBEEF)
    assert len(data) == 6

    parsed = SkyExtensionHeader.parse(data)
    assert parsed.type == ExtType.ARQHandshake
    assert parsed.data.peer_state == 2
    assert parsed.data.identifier == 0xDEADBEEF


def test_tdd_control():
    data = TDDControl(window=892, remaining=456)
    assert len(data) == 5

    parsed = SkyExtensionHeader.parse(data)
    assert parsed.type == ExtType.TDDControl
    assert parsed.data.window == 892
    assert parsed.data.remaining == 456


def test_hmac_sequence_reset():
    data = HMACSequenceReset(sequence=1234)
    assert len(data) == 3

    parsed = SkyExtensionHeader.parse(data)
    assert parsed.type == ExtType.HMACSequenceReset
    assert parsed.data.sequence == 1234


def test_construct_with_extensions():

    extensions = ARQSequence(sequence=123) + ARQRequest(sequence=156, mask=0b1101101)
    data = construct_frame(b"ABCDEF", 123, 3, b"Hello World", extensions=extensions)

    frame = parse_frame(data, parse_extensions=False)
    assert frame.header.identifier == b'ABCDEF'
    assert frame.header.vc == 3
    assert frame.header.sequence == 123
    assert isinstance(frame.header.extensions, bytes)
    assert frame.payload == b'Hello World'

    frame = parse_frame(data, parse_extensions=True)
    assert frame.header.identifier == b'ABCDEF'
    assert frame.header.vc == 3
    assert frame.header.sequence == 123
    assert frame.header.extensions[0].type == ExtType.ARQSequence
    assert frame.header.extensions[0].data.sequence == 123
    assert frame.header.extensions[1].type == ExtType.ARQRequest
    assert frame.header.extensions[1].data.sequence == 156
    assert frame.header.extensions[1].data.mask == 109
    assert frame.payload == b'Hello World'


def test_hmac():
    test_key = bytes(range(32))

    data = construct_frame(b"ABCDEF", 0, 3, b"Hello World", hmac_key=test_key)
    assert data == b'fABCDEF+\x00\x00\x00Hello World\xc5[\x10\xe9'

    parse_frame(data, hmac_key=test_key)

    with pytest.raises(HMACError):
        parse_frame(corrupt(data), hmac_key=test_key)


def _test_fec():
    data = construct_frame(b"OH2AGS", 0, 3, b"Hello World", append_fec=True)
    assert data == b''

    print(construct_frame(b"OH2AGS", 0, 3, b"Hello World", append_golay=True, append_fec=True))

