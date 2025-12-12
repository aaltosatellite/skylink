import uhd
import os
import time
import numpy as np
import pickle
#from mtools.tools_dsp import waterfall_mx
from scipy.signal import firwin
from matplotlib import pyplot as plt
from datetime import datetime as dtime
payloads_in_965 = [
	b'fOH2F1S\x0c%n\x08D\x00\xfa\x00\xfa"\x02\x01\xb9\x01O\x85',
	b'fOH2F1S\x0c%o\x08D\x00\xfa\x00\xb8"\x02\x01\x05!a\xb6',
	b'fOH2F1S\x0c%p\x08D\x00\xfa\x00\xf5"\x02\x01V\'\xf1\xd8',
	b'fOH2F1S\x0c%q\x08D\x00\xfa\x00\xb3"\x02\x01\x87U\xcd5',
	b'fOH2F1S\tq\xec\x05D\x00\xfa\x00u\x0b4\x0b4\x00+\x10\x03\x02g\xc5vO\x00\x00\x01\n\x0b\x95\x03\x00B\x00\xc6\to\xc0[\x04\x08\x00\x10\x01\t\x1f\x00\x05\x04\x1f\x00\x05+17!H1H!\x14\xf6\x9d,',
	b'fOH2F1S\tq\xed\x05D\x00\xfa\x00\x10\x0b4\x0b4\x00a\x10\x03\x05g\xc5vO\x00\x00eAmG\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80?\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00?o\x12\x03;\x84H\x06Z',
	b'fOH2F1S\x0c%r\x08D\x00\xfa\x00\xf6"\x02\x01.S\x9e\xea',
	b'fOH2F1S\x0c%s\x08D\x00\xfa\x00\xb4"\x02\x01\x1e\xc3\x18\x85',
	b'fOH2F1S\tq\xee\x05D\x00\xfa\x00v\x0b4\x0b4\x003\x10\x03\x04g\xc5vO\xb5<\x00\x00\x10\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x16\x01\x16\x01 \x00X\x00d\x04\xd6\xff`\xa4\x15`',
	b'fOH2F1S\tq\xef\x05D\x00\xfa\x00\n\x0b4\x0b4\x00u\x10\x03\x03g\xc5vO\x07\xb7\x03\x00\x1f\x04\x00`\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x1f{\x0f\xf7\r\xc4\r\xca\r\xed\x00\xec\x00\x00\x00\x08\x01\x88\xfd\x88\xfd\x88\xfd\x88\xfd\x88\xff\x8e\xfe\x90\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00X\x00I\x02\x1c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00W\x000\x00\x1c\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\\\x00j\x02\x1c\x00\xef\xd2g\x82',
	b'fOH2F1S\x0c%t\x08D\x00\xfa\x00\xf2"\x02\x01\x8c\xe7\x99l',
	b'fOH2F1S\x0c%u\x08D\x00\xfa\x00\xb0"\x02\x01?d\x16\xb6',
	b'fOH2F1S\x0c%v\x08D\x00\xfa\x00\xf5"\x02\x01v\x14\xca{',
	b'fOH2F1S\x0c%w\x08D\x00\xfa\x00\xb3"\x02\x01.\x1f\x91~',
	b'fOH2F1S\x0c%x\x08D\x00\xfa\x00\xf6"\x02\x01\x9d\x0c\x94\xac',
	b'fOH2F1S\x0c%y\x08D\x00\xfa\x00\xb4"\x02\x01\x85@{r',
	b'fOH2F1S\x0c%z\x08D\x00\xfa\x00\xfa"\x02\x01\x1e\xa5RD',
	b'fOH2F1S\x0c%{\x08D\x00\xfa\x00\xb8"\x02\x01PW\xdc\x80',
	b'fOH2F1S\x0c%|\x08D\x00\xfa\x00\xf6"\x02\x013\xf3\x80\x88',
	b'fOH2F1S\x0c%}\x08D\x00\xfa\x00\xb4"\x02\x01n\xd2\xff\xc0',
	b'fOH2F1S\x08%~\x05D\x00\xfa\x00\xfa\x0b4\x0b4\x00\n\x10\x04\x01g\xc5vZ\x04W\x00\x7f\xbb\xaaN',
]

fpaths = [
	("/home/elmore/datasetit/radiotallenteet/uhf-nayte-76.dat",-124.4e3, None),
	("/home/elmore/datasetit/radiotallenteet/uhf-nayte-96.dat",-124.4e3, None),
	("/home/elmore/datasetit/radiotallenteet/uhf-nayte-237.dat",-123.0e3, None),

	("/home/elmore/datasetit/radiotallenteet/uhf-50_437.0MHz-1000ksps.pickled",-0.0, None),  		# 3 (nothing)
	("/home/elmore/datasetit/radiotallenteet/uhf-812_437.0MHz-1000ksps.pickled",-0.0, None), 		# 4 (nothing)

	("/home/elmore/datasetit/radiotallenteet/uhf-817_437.0MHz-1000ksps.pickled",-123.8e3, None),	# 5
	("/home/elmore/datasetit/radiotallenteet/uhf-969_437.0MHz-1000ksps.pickled",-122.46e3, None),	# 6
	("/home/elmore/datasetit/radiotallenteet/uhf-965_437.0MHz-1000ksps.pickled",-124.0e3, payloads_in_965),	# 7  (21 verified packets.)
	("/home/elmore/datasetit/radiotallenteet/uhf-298_437.0MHz-1000ksps.pickled",-122.5e3, None),	# 8

	("/home/elmore/datasetit/radiotallenteet/uhf-447_437.0MHz-1000ksps.pickled",-122.5e3, None),	# 9
	("/home/elmore/datasetit/radiotallenteet/uhf-195_437.0MHz-1000ksps.pickled",-123.8e3, None),	# 10
	("/home/elmore/datasetit/radiotallenteet/uhf-S_437.0MHz-1000ksps.pickled",-122.5, None),  	# 11  (Kasper-kohinaa & kaksi beaconia)
]



def get_samples(fpath):
	f = open(fpath, "rb")
	rd = f.read()
	f.close()
	samples = pickle.loads(rd)
	assert type(samples) == np.ndarray
	assert samples.dtype in (np.complex64, np.complex128)
	return samples




def show_demod(samples, sr0, baudrate, fshift, x_axis="samples"):
	assert x_axis in ("samples", "time", "symbols")
	samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) * fshift/sr0)
	waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, srate=sr0, plot_and_show=True, y_is_time=True)

	lp_cutoff = 0.625 * baudrate / sr0
	lpfilter = firwin(numtaps=201, cutoff=lp_cutoff, pass_zero=True)

	lowpassed = np.convolve(samples, lpfilter)
	zz = lowpassed * np.conj( np.roll(lowpassed, 1) )
	dmd = np.arctan2(zz.imag, zz.real)

	xx_d = dict()
	xx_d["samples"] = np.arange(len(dmd))
	xx_d["time"] = np.arange(len(dmd)) * (1/sr0)
	xx_d["symbols"] = np.arange(len(dmd)) * (1/sr0) / (1/baudrate)
	xx = xx_d[x_axis]

	fig = plt.figure(figsize=(13,11))
	ax1 = fig.add_subplot(111)
	ax1.plot(xx, dmd )
	ax1.set_xlabel(x_axis)
	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()







def usrp_record(f_tune, sr, t_total, gain=55, show=False):
	"""TX samples based on input arguments"""
	n_record = int(t_total * sr) # number of samples received

	usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
	usrp.set_rx_rate(sr, 0)
	usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(f_tune), 0)
	usrp.set_rx_gain(gain, 0)
	usrp.set_rx_bandwidth(bandwidth=sr, chan=0)
	print("rx frequency: ", usrp.get_rx_freq())
	print("rx samplerate: ", usrp.get_rx_rate())
	print("rx gain range: ", usrp.get_rx_gain_range())
	print("rx gain: ", usrp.get_rx_gain(0))
	print("rx antennas: ", usrp.get_rx_antennas())

	# Set up the stream and receive buffer
	st_args = uhd.usrp.StreamArgs("fc32", "sc16")
	st_args.channels = [0]
	metadata = uhd.types.RXMetadata()
	streamer = usrp.get_rx_stream(st_args)
	recv_buffer = np.zeros((1, 1024*2), dtype=np.complex64)

	# Start Stream
	stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
	stream_cmd.stream_now = True
	streamer.issue_stream_cmd(stream_cmd)

	# Receive Samples
	samples = np.zeros(n_record, dtype=np.complex128)
	print("Engage recording loop.")
	n_received = 0
	t00 = time.perf_counter()
	while n_received < n_record:
		nr = streamer.recv(recv_buffer, metadata)
		if (n_received + nr) < n_record:
			samples[n_received:n_received+nr] = recv_buffer[0,0:nr]
		else:
			samples[n_received:] = recv_buffer[0,0:(n_record - n_received)]
		n_received += nr
	t11 = time.perf_counter()
	print("Avg emipiric sr: ", np.average( n_record / (t11-t00) ))
	print("Total time:", t11-t00 )

	# Stop Stream
	stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.stop_cont)
	streamer.issue_stream_cmd(stream_cmd)
	if show:
		waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=True)
	return samples






def usrp_transmit(samples, f_tune, sr, tx_gain, loop=False):
	assert len(samples.shape) == 1
	n_samples = len(samples)
	if not samples.dtype == np.complex64:
		samples = np.array(samples, dtype=np.complex64)
	samples = np.reshape(samples, (1, len(samples)))

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
		batchlen = min(default_batchlen, n_samples-c)
		nsent = tx_streamer.send(samples[:,c:c+batchlen], tx_metadata)
		assert nsent == batchlen
		c += batchlen
		if c >= n_samples:
			if not loop:
				tx_metadata.end_of_burst = True
				tx_streamer.send(samples[:,0:10], tx_metadata)
				break
			else:
				c = 0
	print("Transmission ended.")








def store_samples(samples, dpath, fname_base, f_tune, sr, comments=""):
	assert os.path.isdir(dpath)
	while True:
		n_letters = 1.0
		triplet = "".join( [chr(x) for x in np.random.randint(65,91, int(n_letters))])
		fpath = dpath + "{}-{}.pkl".format(fname_base, triplet)
		if not os.path.isfile(fpath):
			break
		n_letters += 0.5
	dd = {
		"ts":dtime.now().isoformat(),
		"sr":sr,
		"f_tune":f_tune,
		"samples":samples,
		"comments":comments
	}
	f = open(fpath, "wb")
	f.write(pickle.dumps(dd))
	f.close()
	print("Written into {}".format(fpath))
	return fpath







def load_samples(fpath):
	assert os.path.isfile(fpath)
	f = open(fpath, "rb")
	dd = pickle.loads(f.read())
	f.close()
	if type(dd) == np.ndarray:
		return dd
	if dd["comments"]:
		print("Loaded samples with comments:", dd["comments"])
	return dd["samples"], dd["f_tune"], dd["sr"]














if __name__ == '__main__':
	default_dpath = "/home/aalto/"

	f_tune_ = 437.10e6
	sr_ = 1.5e6
	smpls = usrp_record(f_tune=f_tune_,  sr=sr_, t_total=5.0, gain=55, show=False)
	print("samples max amp:", np.max(np.abs(smpls)) )

	store_samples(samples=smpls, dpath=default_dpath, fname_base="Tallinna_".format(int(time.time())), f_tune=f_tune_, sr=sr_)
	#waterfall_mx(samples=smpls, fftlen=2048, fft_jump=1024, srate=sr_, plot_and_show=True, y_is_time=True)
	#smpls, f_tune_, sr_ = load_samples(fpath=default_dpath + "Clio-B.pkl")
	#print("Loaded {} samples".format(len(smpls)))
	#nn = len(smpls)
	#smpls = smpls[800000:2200000]
	#waterfall_mx(samples=smpls, fftlen=2048, fft_jump=1024, srate=sr_, plot_and_show=True, y_is_time=True)

	#lp_cutoff = 0.625 * baudrate / sr0
	#lpfilter = firwin(numtaps=201, cutoff=lp_cutoff, pass_zero=True)
	#lowpassed = np.convolve(samples, lpfilter)
	#zz = smpls * np.conj( np.roll(smpls, 1) )
	#dmd = np.arctan2(zz.imag, zz.real)

	#xx = np.arange(len(dmd)) * (2*9600/sr_)
	#fig = plt.figure(figsize=(14,10))
	#ax1 = fig.add_subplot(111)
	#ax1.plot(xx[::10], dmd[::10])
	#ax1.grid()
	#fig.set_layout_engine("tight")
	#plt.show()

	#usrp_transmit(samples=smpls, f_tune=f_tune_, sr=sr_, tx_gain=80, loop=False)









