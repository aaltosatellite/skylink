import numpy as np
import time, os
from matplotlib import pyplot as plt
from kuokka.radio_loop import RadioConfig, RadioLoop
from queue import Queue, Empty
import threading

def blind_sink(que:Queue):
	while True:
		try:
			x = que.get(timeout=1.0)
			if (x is "QUIT"):
				return
		except Empty:
			continue




def t1_connect_and_observe():
	config = RadioConfig(mode="usrp", rx_sr=8e6, rx_f_tune=437.0e6, rx_f_center=437.0e6, tx_sr=2e6, tx_f_tune=437.0e6, tx_f_center=437.0e6)
	que_tx_samples_in = Queue(100)
	que_rx_samples_out = Queue(100)
	sinkt = threading.Thread(target=blind_sink, args=(que_rx_samples_out,), daemon=True)
	sinkt.start()
	radioloop = RadioLoop(radio_config=config, que_tx_samples_in=que_tx_samples_in, que_rx_samples_out=que_rx_samples_out)
	radioloop.start()
	time.sleep(8)
	print("Closing.")
	radioloop.close()
	print("Closed")
	#time.sleep(1.0)
	#que_rx_samples_out.put("QUIT")




t1_connect_and_observe()




