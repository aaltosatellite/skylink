import numpy as np
import time, os
from matplotlib import pyplot as plt
from kuokka.radio_loop import RadioConfig, RadioLoop
from queue import Queue, Empty
import threading
from scipy.signal import firwin

def blind_sink(que:Queue):
	while True:
		try:
			x = que.get(timeout=1.0)
			if (x is "QUIT"):
				return
		except Empty:
			continue

def make_powertest_samples(sr, t_array, offset=0.1):
	assert abs(offset) < 0.5
	#lptaps = firwin(121, bw, pass_zero=True)
	nsamples = int(sr*t_array)
	arr = np.exp(2j*np.pi * (np.arange(nsamples)*offset) )
	return arr





def t1_connect_and_observe():
	config = RadioConfig(mode="usrp", rx_sr=8e6, rx_f_tune=437.0e6, tx_sr=2e6, tx_f_tune=170.0e6)
	que_tx_samples_in = Queue(2)
	que_rx_samples_out = Queue(100)
	sinkt = threading.Thread(target=blind_sink, args=(que_rx_samples_out,), daemon=True)
	sinkt.start()
	radioloop = RadioLoop(radio_config=config, que_tx_samples_in=que_tx_samples_in, que_rx_samples_out=que_rx_samples_out)
	radioloop.start()
	time.sleep(40)
	print("Closing.")
	radioloop.close()
	print("Closed")



def t2_continuous_transmit(mode, tx_gain):
	assert mode in ("usrp", "soapy")
	sr00 = 2e6
	f00 = 437.000e6
	config = RadioConfig(mode=mode, rx_sr=sr00, rx_f_tune=f00, tx_sr=sr00, tx_f_tune=f00)
	que_tx_samples_in = Queue(2)
	que_rx_samples_out = Queue(100)
	sinkt = threading.Thread(target=blind_sink, args=(que_rx_samples_out,), daemon=True)
	sinkt.start()
	radioloop = RadioLoop(radio_config=config, que_tx_samples_in=que_tx_samples_in, que_rx_samples_out=que_rx_samples_out)
	radioloop.tx_gain0 = tx_gain
	radioloop.start()
	print("Waiting for radio to start (6s)")
	time.sleep(6.0)
	tx_samples = make_powertest_samples(sr=sr00, t_array=0.5, offset=0.1)
	tx_samples = np.complex64(tx_samples)
	tx_samples = np.reshape(tx_samples, (1,len(tx_samples)))
	i = 0
	while True:
		que_tx_samples_in.put(tx_samples.copy(), timeout=2.0)
		if i==0:
			print("tx...")
		i = (i+1) % 10



def t3_single_burst(mode, tx_gain, burst_duration):
	assert burst_duration < 15.0, "Too long burst for a single samplearray"
	assert mode in ("usrp", "soapy")
	sr00 = 2e6
	f00 = 437.000e6
	config = RadioConfig(mode=mode, rx_sr=sr00, rx_f_tune=f00, tx_sr=sr00, tx_f_tune=f00)
	que_tx_samples_in = Queue(2)
	que_rx_samples_out = Queue(100)
	sinkt = threading.Thread(target=blind_sink, args=(que_rx_samples_out,), daemon=True)
	sinkt.start()
	radioloop = RadioLoop(radio_config=config, que_tx_samples_in=que_tx_samples_in, que_rx_samples_out=que_rx_samples_out)
	radioloop.tx_gain0 = tx_gain
	radioloop.start()
	print("Waiting for radio to start (6s)")
	time.sleep(6.0)
	tx_samples = make_powertest_samples(sr=sr00, t_array=burst_duration, offset=0.1)
	tx_samples = np.complex64(tx_samples)
	tx_samples = np.reshape(tx_samples, (1,len(tx_samples)))
	que_tx_samples_in.put(tx_samples.copy(), timeout=2.0)
	time.sleep(burst_duration+0.2)
	print("Closing.")
	radioloop.close()
	print("Closed")





#t1_connect_and_observe()
t2_continuous_transmit(mode="usrp", tx_gain=4)
t3_single_burst(mode="usrp", tx_gain=0, burst_duration=2.0)



