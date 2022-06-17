"""
"""

import struct
from .fec import SkylinkError

__all__ = [
    "encode_golay24",
    "decode_golay24",
    "encode_golay24_bytes",
    "decode_golay24_bytes",
    "GolayUncorrectable"
]

N = 12
H = ( 0x8008ed, 0x4001db, 0x2003b5, 0x100769, 0x80ed1, 0x40da3,
      0x20b47,  0x1068f,  0x8d1d,   0x4a3b,   0x2477,  0x1ffe )

def parity(v: int) -> int:
    """
    Calculate the parity
    Ref: http://p-nand-q.com/python/algorithms/math/bit-parity.html
    """
    v ^= v >> 1
    v ^= v >> 2
    v = (v & 0x11111111) * 0x11111111
    return (v >> 28) & 1


def popcount(i: int) -> int:
    """
    https://stackoverflow.com/questions/407587/python-set-bits-count-popcount
    """
    assert 0 <= i < 0x100000000
    i = i - ((i >> 1) & 0x55555555)
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333)
    return (((i + (i >> 4) & 0xF0F0F0F) * 0x1010101) & 0xffffffff) >> 24



def encode_golay24(data: int) -> int:
    """
    """
    data &= 0xfff
    s = 0
    for i in range(N):
        s <<= 1
        s |= parity(H[i] & data)
    return ((0xFFF & s) << N) | data


class GolayUncorrectable(SkylinkError):
    pass

class Step8(SkylinkError):
    pass


def decode_golay24(data: int) -> int:
    """
    Decode
    """

    try:
        # Step 1. s = H*r
        s = 0
        for i in range(N):
            s <<= 1
            s |= parity(H[i] & data)

        # Step 2. if w(s) <= 3, then e = (s, 0) and go to step 8
        if popcount(s) <= 3:
            e = s
            e <<= N
            raise Step8()

        # Step 3. if w(s + B[i]) <= 2, then e = (s + B[i], e_{i+1}) and go to step 8
        for i in range(12):
            if popcount(s ^ (H[i] & 0xfff)) <= 2:
                e = s ^ (H[i] & 0xfff)
                e <<= N
                e |= 1 << (N - i - 1)
                raise Step8()

        # Step 4. compute q = B*s
        q = 0
        for i in range(12):
            q <<= 1
            q |= parity((H[i] & 0xfff) & s)

        # Step 5. If w(q) <= 3, then e = (0, q) and go to step 8
        if popcount(q) <= 3:
            e = q
            raise Step8()

        # Step 6. If w(q + B[i]) <= 2, then e = (e_{i+1}, q + B[i]) and got to step 8
        for i in range(12):
            if popcount(q ^ (H[i] & 0xfff)) <= 2:
                e = 1 << (2 * N - i - 1)
                e |= q ^ (H[i] & 0xfff)
                raise Step8()

        # Step 7. r is uncorrectable
        raise GolayUncorrectable()

    except Step8:

        # Step 8. c = r + e
        return (data ^ e) & 0xFFF


def encode_golay24_bytes(v: int) -> bytes:
    """ Encode integer using Golay24 coding and return it as bytes """
    return struct.pack(">I", encode_golay24(v))[1:]

def decode_golay24_bytes(b: bytes) -> int:
    """ Decode Golay24 coded bytes and return it as integer. """
    return decode_golay24(struct.unpack(">I", b"\x00" + b[:3])[0])


if __name__ == "__main__":
    #
    # Short unit test
    #

    data = 200

    r = encode_golay24(data)
    print(f"encoded   {r:06x}")

    r ^= (256|4|1)
    print(f"corrupted {r:06x}")

    rn = decode_golay24(r)
    print(f"decoded   {rn:06x}")
