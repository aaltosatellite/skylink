import secrets
from blake3 import blake3

from .fec import SkylinkError


class HMACError(SkylinkError):
    pass

NONCE_LEN = 2
HMAC_LENGTH = 4


def hmac_verify(data: bytes, key: bytes) -> bytes:
    """
    Check frame authentication

    Args:
        frame: Frame to be authenticated
        key: Authentication key

    Returns:
        Truncated frame without the authentication key.

    Raises:
        `HMACError` if authentication check fails.
    """

    if len(data) < HMAC_LENGTH:
        raise HMACError("Too short frame")

    calculated = blake3(data[:-HMAC_LENGTH], key=key).digest(HMAC_LENGTH)
    received = data[-HMAC_LENGTH:]

    if calculated != received:
        raise HMACError("HMAC does not match expected value!")

    return data[:-HMAC_LENGTH]


def hmac_append(data: bytes, key: bytes) -> bytes:
    """
    Append authentication trailer to frame.

    Args:
        frame: Frame to be authenticated
        key: Authentication key

    Returns:
        An authenticated frame as bytes.
    """

    hmackey = blake3(data, key=key).digest(HMAC_LENGTH)
    return data + hmackey
