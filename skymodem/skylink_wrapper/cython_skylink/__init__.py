
from .c_skylink import SkyConfiguration, SkyLink
from .skylink_process import SkyLinkLoop, EKEY_SKY_ARQ_CONNECTED, EKEY_SKY_ARQ_DISCONNECTED, EKEY_SKY_PAYLOAD

from .c_skylink import num_virtual_channels, num_virtual_channels, max_identity_len, mod_time_ticks, blake3_key_len
from .c_skylink import auth_flag_auth_tx, auth_flag_require_auth, auth_flag_use_crc32, auth_flag_require_seq
from .c_skylink import arq_state_in_init, arq_state_off, arq_state_on
