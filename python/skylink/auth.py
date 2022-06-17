import hashlib
import hmac

from .fec import SkylinkError

class HMACError(SkylinkError):
    pass


HMAC_LENGTH = 8


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

    calculated = hmac.new(key, data[:-HMAC_LENGTH], hashlib.sha256).digest()[:HMAC_LENGTH]
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

    size = len(data) - HMAC_LENGTH
    hmkey = hmac.new(key, data[:size], hashlib.sha256).digest()[:HMAC_LENGTH]

    return data + hmkey
