import threading
from dsp_library.kuokka.dsp_loop import DSPLoop, RXDSPConfig, TXDSPConfig
from dsp_library.kuokka.radio_loop import RadioLoop, RadioConfig
from dsp_library.kuokka.lib_tools import S100_CENTER_FREQUENCY, S100_SYNCHWORD
from queue import Queue, Empty
from application import get_soapy_leecher_receiver_config
import os
from datetime import datetime as dtime
from base64 import b32encode


def signaldata_getter(que_signaldata:Queue):
	i = 1
	fname00 = "Suomi100_intercepted_payloads_{}.dat"
	while fname00.format(i) in os.listdir("."):
		i += 1
	fname = fname00.format(i)
	f = open(fname, "wb")
	print("Storing intercepted payloads into: ", fname)
	while True:
		try:
			ts_unix, rx_f_absolute, power_tuple, baudrate, rx_pl = que_signaldata.get(timeout=1.0)  #self.que_signaldata_out.put((ts_unix, rx_f_absolute, power_tuple, self.rx_dsp_config.baudrate, rx_pl), timeout=1.0)
			(pl_power, noise_power, power_bw) = power_tuple
			ts = dtime.now().isoformat()
			f.write(ts.encode("utf8"))
			f.write(b"   f:")
			f.write(str(float(rx_f_absolute / 1e6 )).encode("utf8") )
			f.write(b" MHz.   payload(base32):")
			f.write(b32encode(rx_pl))
			f.write(b"   P_pl:" + str(float(pl_power)).encode("utf8") + b"   P_noise:"+ str(float(noise_power)).encode("utf8"))
			f.write(b"\n")
			f.flush()
		except Empty:
			pass


def blind_sink(que:Queue):
	while True:
		try:
			que.get(timeout=1.0)
		except Empty:
			pass


def main():
	f_tune 		= 436.000e6
	f_center 	= S100_CENTER_FREQUENCY
	#sr0 = abs(f_center-f_tune)
	#radio_config = RadioConfig(mode="soapy", rx_sr=sr0, rx_f_tune=f_tune, tx_sr=sr0, tx_f_tune=f_tune)
	#rx_dsp_config = RXDSPConfig(rx_sr0=sr0, rx_f_tune=f_tune, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=1024*16)
	#tx_dsp_config = TXDSPConfig(tx_sr0=sr0, tx_f_tune=f_tune, tx_f_center=f_center, baudrate=9600)
	rx_dsp_config, tx_dsp_config, radio_config = get_soapy_leecher_receiver_config(f_center=f_center, baudrate=9600, f_tune=f_tune, sr_hardware=8e6, max_signal_bw=9600*4.0)

	rx_dsp_config.synchword = S100_SYNCHWORD

	q_samples_radio_to_dsp 		= Queue(256)
	q_payloads_out 				= Queue(256)
	q_tx_payloads_to_dsp 		= Queue(256) # unused
	q_samples_dsp_to_radio 		= Queue(256) # unused
	q_signaldata_out 			= Queue(256)
	radioloop = RadioLoop(radio_config=radio_config, que_tx_samples_in=q_samples_dsp_to_radio, que_rx_samples_out=q_samples_radio_to_dsp)
	dsploop = DSPLoop(rx_dsp_config=rx_dsp_config, tx_dsp_config=tx_dsp_config, que_rx_samples_in=q_samples_radio_to_dsp, que_rx_payloads_out=q_payloads_out,
					  que_tx_payloads_in=q_tx_payloads_to_dsp, que_tx_samples_out=q_samples_dsp_to_radio, que_signaldata_out=q_signaldata_out)

	th1 = threading.Thread(target=blind_sink, args=(q_payloads_out,))
	th1.start()

	th2 = threading.Thread(target=signaldata_getter, args=(q_signaldata_out,))
	th2.start()

	dsploop.start()
	radioloop.start()





if __name__ == '__main__':
	main()











