import numpy as np
import uhd
import time, threading



def usrp_transmit(f_carrier, tx_gain, loop=False):
	f_tune = f_carrier - 150e3
	sr = 1e6
	nsamples = int(sr*0.25)
	samples = np.zeros((1, nsamples), dtype=np.complex64)
	samples[0,:] = np.exp(2j * np.pi * np.arange(nsamples) * (1/sr) * (f_carrier - f_tune))

	usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
	usrp.set_rx_rate(sr)
	usrp.set_tx_rate(sr)
	usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(f_tune), 0)
	usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(f_tune), 0)
	usrp.set_tx_gain(tx_gain)
	print("tx frequency:  {} MHz".format(round(usrp.get_tx_freq(0)*1e-6, 4)))
	print("tx gain range: {}".format(usrp.get_tx_gain_range(0)))

	tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
	#stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
	tx_stream_args.channels = [0]
	tx_streamer = usrp.get_tx_stream(tx_stream_args)
	tx_metadata = uhd.types.TXMetadata()
	print("tx_streamer max_num_samps: {}".format( tx_streamer.get_max_num_samps()))

	c = 0
	default_batchlen = 1024*2
	print("Transmitting")
	while True:
		batchlen = min(default_batchlen, nsamples-c)
		nsent = tx_streamer.send(samples[:,c:c+batchlen], tx_metadata)
		assert nsent == batchlen
		c += batchlen
		if c >= nsamples:
			if not loop:
				tx_metadata.end_of_burst = True
				tx_streamer.send(samples[:,0:10], tx_metadata)
				break
			else:
				c = 0
	print("Transmission ended.")










if __name__ == '__main__':
	usrp_transmit(f_carrier=437.0e6, tx_gain=60.0, loop=True)




















