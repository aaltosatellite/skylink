import random
from typing import Optional


def random_frame(n: Optional[int]=None) -> bytes:
    """ Generate a random frame """
    if n is None:
        n = random.randint(34, 255)
    return bytes([ random.randint(0, 255) for _ in range(n) ])


def corrupt(raw: bytes, n: Optional[int]=None) -> bytes:
    """ Flip N bits in the frame """
    raw = bytearray(raw)
    if n is None:
        n = random.randint(1, 20)
    for _ in range(n):
        raw[random.randint(0, len(raw) - 1)] ^= (1 << random.randint(0, 7))
    return bytes(raw)
