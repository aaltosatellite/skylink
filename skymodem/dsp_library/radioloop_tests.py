import numpy as np
import time, os
from matplotlib import pyplot as plt
from kuokka.radio_loop import RadioConfig, RadioLoop
from queue import Queue, Empty
import threading
from scipy.signal import firwin


def blind_sink(que: Queue):
    while True:
        try:
            x = que.get(timeout=1.0)
            if (x == "QUIT"):
                return
        except Empty:
            continue


def make_powertest_samples(sr, t_array, offset=0.1):
    assert abs(offset) < 0.5
    # lptaps = firwin(121, bw, pass_zero=True)
    nsamples = int(sr * t_array)
    arr = np.exp(2j * np.pi * (np.arange(nsamples) * offset))
    return arr


def t1_connect_and_observe():
    config = RadioConfig(mode="usrp", rx_samplerate=8e6, rx_tune_frequency=437.0e6, tx_samplerate=2e6, tx_tune_frequency=170.0e6, rx_gain=40, tx_gain=0)
    queue_tx_samples_from_dsp = Queue(2)
    queue_rx_samples_to_dsp = Queue(100)
    sinkt = threading.Thread(target=blind_sink, args=(queue_rx_samples_to_dsp,), daemon=True)
    sinkt.start()
    radioloop = RadioLoop(radio_config=config, queue_tx_samples_from_dsp=queue_tx_samples_from_dsp, queue_rx_samples_to_dsp=queue_rx_samples_to_dsp)
    radioloop.start()
    time.sleep(40)
    print("Closing.")
    radioloop.close()
    print("Closed")


def t2_continuous_transmit(mode, tx_gain):
    assert mode in ("usrp", "soapy")
    sr00 = 2e6
    f00 = 437.000e6
    config = RadioConfig(mode=mode, rx_samplerate=sr00, rx_tune_frequency=f00, tx_samplerate=sr00, tx_tune_frequency=f00, rx_gain=40, tx_gain=tx_gain)
    queue_tx_samples_from_dsp = Queue(2)
    queue_rx_samples_to_dsp = Queue(100)
    sinkt = threading.Thread(target=blind_sink, args=(queue_rx_samples_to_dsp,), daemon=True)
    sinkt.start()
    radioloop = RadioLoop(radio_config=config, queue_tx_samples_from_dsp=queue_tx_samples_from_dsp, queue_rx_samples_to_dsp=queue_rx_samples_to_dsp)
    radioloop.start()
    print("Waiting for radio to start (5s)")
    time.sleep(5.0)
    tx_samples = make_powertest_samples(sr=sr00, t_array=0.5, offset=0.1)
    tx_samples = np.complex64(tx_samples)
    tx_samples = np.reshape(tx_samples, (1, len(tx_samples)))
    i = 0
    while True:
        queue_tx_samples_from_dsp.put(tx_samples.copy(), timeout=2.0)
        if i == 0:
            print("tx...")
        i = (i + 1) % 10


def t3_single_burst(mode, tx_gain, burst_duration):
    assert burst_duration < 15.0, "Too long burst for a single samplearray"
    assert mode in ("usrp", "soapy")
    sr00 = 2e6
    f00 = 437.000e6
    config = RadioConfig(mode=mode, rx_samplerate=sr00, rx_tune_frequency=f00, tx_samplerate=sr00, tx_tune_frequency=f00, rx_gain=40, tx_gain=tx_gain)
    queue_tx_samples_from_dsp = Queue(2)
    queue_rx_samples_to_dsp = Queue(100)
    sinkt = threading.Thread(target=blind_sink, args=(queue_rx_samples_to_dsp,), daemon=True)
    sinkt.start()
    radioloop = RadioLoop(radio_config=config, queue_tx_samples_from_dsp=queue_tx_samples_from_dsp, queue_rx_samples_to_dsp=queue_rx_samples_to_dsp)
    radioloop.start()
    print("Waiting for radio to start (2s)")
    time.sleep(2.0)
    tx_samples = make_powertest_samples(sr=sr00, t_array=burst_duration, offset=0.1)
    tx_samples = np.complex64(tx_samples)
    tx_samples = np.reshape(tx_samples, (1, len(tx_samples)))
    queue_tx_samples_from_dsp.put(tx_samples.copy(), timeout=2.0)
    time.sleep(burst_duration + 0.2)
    print("Closing.")
    radioloop.close()
    print("Closed")


if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--test", "-t", type=int, default=2, choices=[1, 2, 3],
                                            help="Test mode: 1 connect & observe, 2 continuous TX (default), 3 single burst TX")
    parser.add_argument("--mode", "-m", type=str, default="usrp", choices=("usrp", "soapy"),
                                            help="Run test by directly attaching to the USRP (default) or via SoapyShared.")
    parser.add_argument("--tx_gain", "-g", type=float, default=70,
                                            help="TX Gain setting for the USRP: 0.0 - 89.75 [dB]. Default is 70.")
    _args = parser.parse_args(sys.argv[1:])
    assert _args.tx_gain >= 0.0, f"tx_gain must be between 0.0 and 89.75 [dB]. Was {_args.tx_gain}"
    assert _args.tx_gain <= 89.75, f"tx_gain must be between 0.0 and 89.75 [dB]. Was {_args.tx_gain}"

    match _args.test:
        case 1:
            t1_connect_and_observe()
        case 2:
            t2_continuous_transmit(mode=_args.mode, tx_gain=_args.tx_gain)
        case 3:
            t3_single_burst(mode=_args.mode, tx_gain=_args.tx_gain, burst_duration=2.0)
