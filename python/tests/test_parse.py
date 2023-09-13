import pytest
from skylink import parse_frame, SkylinkError


def test_parsing():

    # "hello world" without extension or authentication
    frame = parse_frame(b'fABCDEF#\x00\x00\x00Hello world')
    assert frame.header.version == 12
    assert frame.header.identifier == b'ABCDEF'
    assert frame.header.flags.has_payload == True
    assert frame.header.flags.arq_on == False
    assert frame.header.flags.is_authenticated == False # TODO
    assert frame.header.vc == 3
    assert frame.header.sequence == 0
    assert frame.payload == b'Hello world'
    assert frame.auth == None

    # "hello world" with authentication
    frame = parse_frame(b'fABCDEF*\x05\x00\x00T\x01\xf4\x01\xebHello world\xde\xad\xbe\xef')
    assert frame.header.version == 12
    assert frame.header.identifier == b'ABCDEF'
    assert frame.header.flags.has_payload == True
    assert frame.header.flags.arq_on == False
    assert frame.header.flags.is_authenticated == True
    assert frame.header.vc == 2
    assert frame.header.sequence == 0
    assert frame.payload == b'Hello world'
    assert frame.auth == b'\xde\xad\xbe\xef'


def test_variable_identity_lengths():

    # TODO: Turn off HMAC flag

    # Identifier length = 7
    frame = parse_frame(b'\x67ABCDEFG*\x05\x00\x00T\x01\xf4\x01\xeb\xab!Hello world')
    assert frame.header.version == 12
    assert frame.header.identifier == b'ABCDEFG'
    assert frame.header.vc == 2
    assert frame.header.sequence == 1
    assert frame.payload == b'Hello world'

    # Identifier length = 5
    frame = parse_frame(b'\x65ABCDE*\x05\x00\xffT\x01\xf4\x01\xeb\xab!Hello world')
    assert frame.header.version == 12
    assert frame.header.identifier == b'ABCDE'
    assert frame.header.vc == 2
    assert frame.header.sequence == 255
    assert frame.payload == b'Hello world'

    # Identifier length = 3
    frame = parse_frame(b'\x63ABC*\x05\x00\x00T\x01\xf4\x01\xeb\xab!Hello world')
    assert frame.header.version == 12
    assert frame.header.identifier == b'ABC'
    assert frame.header.vc == 2
    assert frame.header.sequence == 256
    assert frame.payload == b'Hello world'

    # Identifier length = 1
    frame = parse_frame(b'\x61A*\x05\x01\x00T\x01\xf4\x01\xeb\xab!Hello world')
    assert frame.header.version == 12
    assert frame.header.identifier == b'A'
    assert frame.header.vc == 2
    assert frame.header.sequence == 0
    assert frame.payload == b'Hello world'
    #assert frame.extensions

    # Identifier length = 0
    with pytest.raises(SkylinkError):
        frame = parse_frame(b'\x60*\x05\x00\x03T\x01\xf4\x01\xeb\xab!Hello world')
        print(frame)

    # Wrong version (Identifier length = 8)
    with pytest.raises(SkylinkError):
        frame = parse_frame(b'\x68ABCDEFGH*\x05\x00\x10T\x01\xf4\x01\xeb\xab!Hello world')
        print(frame)


def test_parse_extensions():

    frame = parse_frame(b'fABCDEF#\x06\x00{\x10{!\x9c\x00mHello World', parse_extensions=True)
    assert frame.header.identifier == b'ABCDEF'
    assert frame.header.vc == 2
    assert frame.header.sequence == 0
    assert frame.payload == b'Hello world'
    assert len(frame.extensions) == 2
    assert frame.extensions[0].type == 0
    assert frame.extensions[0].data.sequence == 123
    assert frame.extensions[1].type == 1
    assert frame.extensions[1].data.sequence == 156
    assert frame.extensions[1].data.mask == 109
