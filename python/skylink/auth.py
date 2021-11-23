

import hashlib
import hmac

def check_authentication(frame: bytes, key: bytes, required: bool=False) -> bool:
    """
    Check frame authentication

    Args:
        frame: Frame to be authenticated
        key: Authentication
        required: Is authentication required for passing the check

    Returns:
        True if the frame passed the authentication check.
    """

    if (frame[43] & 0x10) == 0: # TODO
        return not required # Not authenticated

    calculated = hmac.new(key, msg=frame[:-8], digestmod=hashlib.sha256).digest()

    return calculated[:8] == frame[-8:]
