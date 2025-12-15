import time
import os
from skylink_wrapper.cython_skylink import SkyConfiguration
from skylink_wrapper.cython_skylink import auth_flag_auth_tx, auth_flag_require_auth, auth_flag_require_seq
from skymodem import SkyModem
from dsp_library.kuokka.radio_loop import RadioConfig
from dsp_library.kuokka.dsp_loop import TXDSPConfig
from dsp_library.kuokka.lib_tools import get_doppler_low_high, usrp_B200_valid_samplerates, fractional_resampler_f_max_undisturbed
from dsp_library.kuokka.lib_receiver import RXDSPConfig




def get_usrp_receiver_config(f_center, baudrate, max_signal_bw, rx_gain, tx_gain):
    from dsp_library.kuokka.lib_tools import determine_ftune_and_min_sr
    f_center_min, f_center_max = get_doppler_low_high(f_center=f_center, v_relative=7500.0*2)
    f_tune, minimum_samplerate = determine_ftune_and_min_sr(f_center_min=f_center_min, f_center_max=f_center_max, max_signal_bandwidth=max_signal_bw)
    assert minimum_samplerate < 3e6
    sr0 = 1e6
    if sr0 < minimum_samplerate:
        sr0 = [float(x) for x in sorted(usrp_B200_valid_samplerates) if x >= minimum_samplerate][0]
    print("Calculated minimum samplerate at {} ks/s".format( round(1.0e-3 * minimum_samplerate, 1) ))
    print("Using usrp radio config of: f_tune={} MHz,   sr0={} Ms/s".format( round(f_tune*1e-6, 3), round(sr0*1e-6, 3) ))
    radio_config 	= RadioConfig(mode="usrp", rx_samplerate=sr0, rx_tune_frequency=f_tune, tx_samplerate=sr0, tx_tune_frequency=f_tune, rx_gain=rx_gain, tx_gain=tx_gain)
    rx_dsp_config 	= RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_center_frequency=f_center, baudrate=baudrate, bufferlen=800000, batch_maxlen=1024 * 16)
    tx_dsp_config 	= TXDSPConfig(tx_samplerate=sr0, tx_tune_frequency=f_tune, tx_center_frequency=f_center, baudrate=baudrate)
    return rx_dsp_config, tx_dsp_config, radio_config



def get_soapy_leecher_receiver_config(soapy_selection, f_center, baudrate, f_tune, sr_hardware, max_signal_bw, rx_gain, tx_gain):
    f_center_min, f_center_max = get_doppler_low_high(f_center=f_center, v_relative=7500.0*2)
    f_center_min = f_center_min - max_signal_bw * 0.6
    f_center_max = f_center_max + max_signal_bw * 0.6
    plateu_minimum_halfwidth = max( abs(f_tune - f_center_min), abs(f_tune - f_center_max) )
    minimum_samplerate = int(2 * plateu_minimum_halfwidth)
    sr_leecher = max(1e6, 1e6*int(minimum_samplerate/1e6))
    while True:
        bw_leecher = 0.45 * (sr_leecher / sr_hardware)  	# This is the way SoapyShared computes the resampler filter length. (see SoapyLeecher.cpp:150)
        semilen_leecher = int(round(3.5 / bw_leecher))		# This is the way SoapyShared computes the resampler filter length. (see SoapyLeecher.cpp:150)
        f_max_undisturbed = fractional_resampler_f_max_undisturbed(sr0=sr_hardware, sr1=sr_leecher, halflen=semilen_leecher, f_cutoff_coeff=0.45)
        if f_max_undisturbed >= plateu_minimum_halfwidth:
            break
        sr_leecher += int(100e3)
    print("Calculated minimum samplerate at {} ks/s".format( round(1.0e-3 * minimum_samplerate, 1) ))
    print("Calculated necessary samplerate at {} ks/s".format( round(1.0e-3 * sr_leecher, 1) ))
    print("Using soapy-leecher radio config of: f_tune={} MHz,   sr0={} Ms/s".format( round(f_tune*1e-6, 3), round(sr_leecher*1e-6, 3) ))
    radio_config 	= RadioConfig(mode=soapy_selection, rx_samplerate=sr_leecher, rx_tune_frequency=f_tune, tx_samplerate=sr_leecher, tx_tune_frequency=f_tune, rx_gain=rx_gain, tx_gain=tx_gain)
    rx_dsp_config 	= RXDSPConfig(rx_samplerate=sr_leecher, rx_tune_frequency=f_tune, rx_center_frequency=f_center, baudrate=baudrate, bufferlen=800000, batch_maxlen=1024 * 16)
    tx_dsp_config 	= TXDSPConfig(tx_samplerate=sr_leecher, tx_tune_frequency=f_tune, tx_center_frequency=f_center, baudrate=baudrate)
    return rx_dsp_config, tx_dsp_config, radio_config



def read_keys_from_header(file_path: str):
    """
    Function for reading the HMAC key from a header file.
    Uses regular expressions to extract the key from a C-style array definition.

    Args:
            file_path: str		Path to secret file containing the authentication keys
            key_name: str 		What is the key name in the secret file
    Returns:
            byte_array: The HMAC key as a bytearray.
    """
    keys = []
    for keylabel in ["uplink_key", "downlink_key", "service_key"]:
        with open(file_path, 'r') as file:
            content = file.read()
            # Regex to match the byte array in the .h file, allowing for line breaks and spaces
            import re
            match = re.search(keylabel + r'\s*\[\d+\]\s*=\s*\{([^}]+)\};', content, re.DOTALL)
            if not match:
                raise ValueError(f"Key not found in the header file{file_path}")
            # Extract the bytes and convert them to a byte array
            byte_values = match.group(1).replace('\n', '').split(',')
            byte_array = bytes(int(b.strip(), 16) for b in byte_values if b.strip())
            if len(byte_array) != 32:
                raise ValueError("Key is not 32 bytes long.")
            keys.append(byte_array)
    return keys





if __name__ == '__main__':
    rx_dsp_config_, tx_dsp_config_, radio_config_ = None, None, None
    import sys
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", "-m", type=str, default="usrp", choices=("usrp", "soapy", "soapy-buu"), help="Operation mode: attach directly to the USRP (default) or via SoapyShared: 'soapy' for main UHF and 'soapy-buu' for backup.", required=False)
    parser.add_argument("--vc_base", "-vc", type=int, default=7100, help="Virtual Channel base. Default 7100.", required=False)
    parser.add_argument("--center_freq", "-cf", "-f", type=float, default=437.025e6, help="Center frequency used for communications [Hz]. Default 437.025 MHz (dev frequency).")
    parser.add_argument("--rx_gain", "-rg", type=float, default=40, help="Reception Gain setting for the USRP: 0 - 76 [dB]. Default is 40.", required=False)
    parser.add_argument("--tx_gain", "-tg", type=float, default=80, help="TX Gain setting for the USRP: 0.0 - 89.75 [dB]. Default is 80.", required=False)
    parser.add_argument("--auth", "-a", type=str, default="dev", choices=("dev", "spare", "fm"), help="Authentication key choices: 'dev' development (default), 'spare' Flight Model Spare, and 'fm' Flight Model.", required=False)
    parser.add_argument("--multimode", "-mm", action="store_true", help="Run modem in 'multimode', which means that all baudrates (9600, 19200, 38400) are being received.")
    parser.add_argument("--doppler", "-d", action="store_true", help="Run modem with Doppler compensation; the implementation varies.")
    parser.add_argument("--doppler-tle", "-dt", action="store_true", help="Run modem with Doppler compensation based on TLE data; requires internet connection to fetch latest TLEs.")
    parser.add_argument("--no-follow", "-nf", action="store_true", help="Disable all frequency corrections from the modem. Overwrites the previous two options.")
    _args = parser.parse_args(sys.argv[1:])
    vc_base = _args.vc_base
    assert vc_base >= 1000
    assert vc_base < 60000
    assert _args.rx_gain >= 0, f"rx_gain must be between 0 and 76 [dB]. Was {_args.rx_gain}"
    assert _args.rx_gain <= 76, f"rx_gain must be between 0 and 76 [dB]. Was {_args.rx_gain}"
    assert _args.tx_gain >= 0.0, f"tx_gain must be between 0.0 and 89.75 [dB]. Was {_args.tx_gain}"
    assert _args.tx_gain <= 89.75, f"tx_gain must be between 0.0 and 89.75 [dB]. Was {_args.tx_gain}"
    if _args.auth == "fm":
        # Safeguards for GS hardware: especially switch.
        assert _args.center_freq >= 436e6, f"center_freq must be between 436 and 438 MHz. Was {_args.center_freq}"
        assert _args.center_freq <= 438e6, f"center_freq must be between 436 and 438 MHz. Was {_args.center_freq}"

    if os.path.isfile("secret.h") and _args.auth == "fm":
        print("[AUTH] Using FM authentication keys.")
        uplink_key, downlink_key, service_key = read_keys_from_header("secret.h")
    elif os.path.isfile("sparesecret.h") and _args.auth == "spare":
        print("[AUTH] Using FM Spare authentication keys.")
        uplink_key, downlink_key, service_key = read_keys_from_header("sparesecret.h")
    else:
        print("[AUTH] External secret not available or selected, using development keys.")
        # FS1p Development keys. Downlink, uplink, and service in order.
        uplink_key = bytes([
                0xc2, 0x54, 0x70, 0x55, 0x64, 0xa1, 0xba, 0x34,
                0x84, 0x36, 0xb2, 0x5b, 0xfd, 0x97, 0x4c, 0x85,
                0xb5, 0x29, 0x51, 0x15, 0x42, 0xfd, 0xe8, 0x90,
                0x10, 0x0a, 0xe1, 0xb6, 0xd5, 0x6b, 0xf9, 0xfd
        ])
        downlink_key = bytes([
                0x41, 0xb7, 0xb5, 0xff, 0x40, 0x18, 0x8a, 0x78,
                0x05, 0x82, 0x04, 0x3d, 0x1f, 0xda, 0xe4, 0x85,
                0xdd, 0x85, 0x87, 0xef, 0x82, 0xd6, 0x49, 0xa8,
                0x01, 0x3f, 0xb2, 0x80, 0x5c, 0xc1, 0x69, 0x0e
        ])
        service_key = bytes([
                0x8b, 0x16, 0x3a, 0x37, 0xd7, 0xda, 0x14, 0xde,
                0xc1, 0x82, 0x60, 0xf3, 0xc6, 0x7c, 0xe0, 0xbe,
                0x6f, 0x66, 0xfb, 0x7a, 0x36, 0xbd, 0x1e, 0x6d,
                0x51, 0xf2, 0xed, 0xe4, 0x45, 0x65, 0x56, 0x6b
        ])
    if uplink_key == b"":
        print("[EXIT] Check HMAC Key!")
        exit()
    # Different keys for uplink, downlink, and service channel
    hmac_keys = [uplink_key, downlink_key, service_key]
    ###print(f"[AUTH] HMAC keys: \n{uplink_key.hex()},\n{downlink_key.hex()},\n{service_key.hex()}") #DEBUG!!

    skylink_config_ = SkyConfiguration(identity=b"PyGS")

    #* DO NOT MODIFY FOLLOWING CONFIGURATIONS IN ANY SITUATION!!! *#
    skylink_config_.vc[0].require_authentication = auth_flag_require_auth | auth_flag_require_seq | auth_flag_auth_tx
    skylink_config_.vc[1].require_authentication = auth_flag_require_auth | auth_flag_require_seq | auth_flag_auth_tx
    skylink_config_.vc[2].require_authentication = auth_flag_require_auth | auth_flag_auth_tx
    skylink_config_.vc[3].require_authentication = 0 # disable auth etc on radio amateur channel

    # different keys shall be used for attack vector prevention (eg. replay)
    # sequence counter enable for non service channels
    # also if downlink key is different to uplink, downlink key sharing is possible for third party verification without losing security

    # channel 0 key configuration
    skylink_config_.vc[0].rx_key = 1  # uses the downlink key [key index 1] (satellite sending)
    skylink_config_.vc[0].tx_key = 0  # uses uplink key [key index 0] (satellite receiving)

    # channel 1 key configuration
    skylink_config_.vc[1].rx_key = 1  # uses the downlink key [key index 1] (satellite sending)
    skylink_config_.vc[1].tx_key = 0  # uses uplink key [key index 0] (satellite receiving)

    # service channel (2) configuration
    skylink_config_.vc[2].rx_key = 2  # separate service channel beacon
    skylink_config_.vc[2].tx_key = 2  # uses the same key for both channel as there is no beacon

    # Radio amateur channel, key auth disabled
    skylink_config_.vc[3].rx_key = 0  # setting does not change it
    skylink_config_.vc[3].tx_key = 0
    #* DO NOT MODIFY ABOVE CONFIGURATIONS IN ANY SITUATION!!! *#


    if "soapy" in _args.mode:
        ftune_correction = 40e3
        print(f"[INIT] Using SoapyShared mode, center_freq={_args.center_freq} Hz, and 'ftune_correction'={ftune_correction} Hz")
        rx_dsp_config_, tx_dsp_config_, radio_config_ = get_soapy_leecher_receiver_config(soapy_selection=_args.mode, f_center=_args.center_freq + 0e3, baudrate=9600, f_tune=436e6+ftune_correction, sr_hardware=8e6, max_signal_bw=9600*4*1.2, rx_gain=_args.rx_gain, tx_gain=_args.tx_gain)
    else:
        assert _args.mode == "usrp"
        print(f"[INIT] Using USRP mode, center_freq={_args.center_freq} Hz, and no 'ftune_correction'.")
        rx_dsp_config_, tx_dsp_config_, radio_config_ = get_usrp_receiver_config(f_center=_args.center_freq + 0e3, baudrate=9600, max_signal_bw=9600*4*1.2, rx_gain=_args.rx_gain, tx_gain=_args.tx_gain)

    #amqp_broker_addr_ = "amqp://guest:guest@localhost:5672"
    amqp_broker_addr_ = "amqp://modem:fs1pmodem@192.168.10.2:5672"
    #amqp_broker_addr_ = None
    modem = SkyModem(rx_dsp_config=rx_dsp_config_, tx_dsp_config=tx_dsp_config_, radio_config=radio_config_, skylink_config=skylink_config_, hmac_key_list=hmac_keys, vc_port_base=vc_base, amqp_broker_addr=amqp_broker_addr_)
    modem.start(multimode=_args.multimode)

    if _args.doppler:
        modem.dsp_loop.set_doppler_correction(True)
        print("[INIT] Doppler correction enabled")
    if _args.doppler_tle:
        modem.dsp_loop.set_tle_doppler_correction(True)
        modem.dsp_loop.set_doppler_correction(False)
        print("[INIT] TLE-based Doppler correction enabled")
    if _args.no_follow:
        modem.dsp_loop.set_tle_doppler_correction(False)
        modem.dsp_loop.set_doppler_correction(False)
        modem.dsp_loop.set_do_frequency_following(False)
        print("[INIT] All frequency corrections disabled!")

    try:
        while True:
            #print(threading.active_count(), "threads active")
            time.sleep(0.5)
            if not modem.is_ok():
                print("[EXIT] Modem is_ok() failed. Exiting.")
                break
    except KeyboardInterrupt:
        pass
    modem.close()
    sys.exit(0)
