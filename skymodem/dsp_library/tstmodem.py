import numpy as np
import time, os, sys
from kuokka.dsp_loop import DSPLoop, TXDSPConfig, RXDSPConfig
from kuokka.radio_loop import RadioLoop, RadioConfig
from queue import Queue, Empty
import threading

def que_depleater(que:Queue, killcmd):
	while True:
		try:
			x = que.get(timeout=0.2)
			if x is killcmd:
				print("que depleter exits with killcmd")
				return
		except Empty:
			pass




class TstModem:
	def __init__(self):
		srbase = 2e6
		self.srbase = srbase
		self.rx_dspconfig = RXDSPConfig(rx_sr0=srbase, rx_f_tune=437.000e6, rx_f_center=437.400e6, baudrate=9600, bufferlen=800000, batch_maxlen=1024*8)
		self.tx_dspconfig = TXDSPConfig(tx_sr0=srbase, tx_f_tune=437.000e6, tx_f_center=437.400e6, baudrate=9600)
		self.radioconfig = RadioConfig(mode="usrp", rx_sr=srbase, rx_f_tune=437.000e6, rx_f_center=437.400e6, tx_sr=srbase, tx_f_tune=437.000e6, tx_f_center=437.400e6)
		self.que_rx_samples_in  	= Queue(256)
		self.que_rx_pls_out     	= Queue(256)
		self.que_tx_pls_in 		= Queue(256)
		self.que_tx_samples_out 	= Queue(256)
		self.que_signaldata_out 	= Queue(256)
		self.dsploop = None
		self.radioloop = None
		self.depleters = list()

	def run_both(self):
		self.dsploop = DSPLoop(rx_dsp_config=self.rx_dspconfig, tx_dsp_config=self.tx_dspconfig, que_rx_samples_in=self.que_rx_samples_in, que_rx_payloads_out=self.que_rx_pls_out,
							   que_tx_payloads_in=self.que_tx_pls_in, que_tx_samples_out=self.que_tx_samples_out, que_signaldata_out=self.que_signaldata_out)
		self.radioloop = RadioLoop(radio_config=self.radioconfig, que_tx_samples_in=self.que_tx_samples_out, que_rx_samples_out=self.que_rx_samples_in)
		depleter1 = threading.Thread(target=que_depleater, args=(self.que_signaldata_out,"KILLCMD"), daemon=True)
		depleter2 = threading.Thread(target=que_depleater, args=(self.que_rx_pls_out,"KILLCMD"), daemon=True)
		depleter1.start()
		depleter2.start()
		self.depleters.append(depleter1)
		self.depleters.append(depleter2)
		self.dsploop.start()
		self.radioloop.start()


	def run_radio(self):
		self.radioloop = RadioLoop(radio_config=self.radioconfig, que_tx_samples_in=self.que_tx_samples_out, que_rx_samples_out=self.que_rx_samples_in)
		depleter1 = threading.Thread(target=que_depleater, args=(self.que_rx_samples_in,"KILLCMD"), daemon=True)
		depleter1.start()
		self.depleters.append(depleter1)
		self.radioloop.start()


	def close(self):
		if not (self.dsploop is None):
			self.dsploop.close()
		if not (self.radioloop is None):
			self.radioloop.close()

	def transmit_pl(self, length):
		self.que_tx_pls_in.put(os.urandom(length), timeout=1.0)

	def transmit_noise_nsamples(self, nsamples):
		arr = np.zeros((1,nsamples), dtype=np.complex64)
		arr[0,:] = np.random.normal(0, 0.001, nsamples) + np.random.normal(0, 0.001, nsamples)*1j
		self.que_tx_samples_out.put(arr, timeout=1.0)

	def transmit_noise_time(self, t_secs):
		nsamples = int(t_secs * self.srbase)
		self.transmit_noise_nsamples(nsamples=nsamples)













