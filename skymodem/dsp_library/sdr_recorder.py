import uhd
import os
import time
import numpy as np
import pickle
from mtools.tools_dsp import waterfall_mx
from scipy.signal import firwin
from matplotlib import pyplot as plt



fpaths = [
	("/home/elmore/datasetit/radiotallenteet/uhf-nayte-76.dat",-124.4e3),
	("/home/elmore/datasetit/radiotallenteet/uhf-nayte-96.dat",-124.4e3),
	("/home/elmore/datasetit/radiotallenteet/uhf-nayte-237.dat",-123.0e3),

	("/home/elmore/datasetit/radiotallenteet/uhf-50_437.0MHz-1000ksps.pickled",-0.0),  		# 3 (nothing)
	("/home/elmore/datasetit/radiotallenteet/uhf-812_437.0MHz-1000ksps.pickled",-0.0), 		# 4 (nothing)

	("/home/elmore/datasetit/radiotallenteet/uhf-817_437.0MHz-1000ksps.pickled",-123.8e3),	# 5
	("/home/elmore/datasetit/radiotallenteet/uhf-969_437.0MHz-1000ksps.pickled",-122.46e3),	# 6
	("/home/elmore/datasetit/radiotallenteet/uhf-965_437.0MHz-1000ksps.pickled",-124.0e3),	# 7  (21 verified packets.)
	("/home/elmore/datasetit/radiotallenteet/uhf-298_437.0MHz-1000ksps.pickled",-122.5e3),	# 8

	("/home/elmore/datasetit/radiotallenteet/uhf-447_437.0MHz-1000ksps.pickled",-122.5e3),	# 9
	("/home/elmore/datasetit/radiotallenteet/uhf-195_437.0MHz-1000ksps.pickled",-123.8e3),	# 10
	("/home/elmore/datasetit/radiotallenteet/uhf-S_437.0MHz-1000ksps.pickled",-122.5),  	# 11  (Kasper-kohinaa & kaksi beaconia)
]



def get_samples(fpath):
	f = open(fpath, "rb")
	rd = f.read()
	f.close()
	samples = pickle.loads(rd)
	assert type(samples) == np.ndarray
	assert samples.dtype in (np.complex64, np.complex128)
	return samples









def record_and_save(f_center, sr, t_total, target_dpath = "/home/elmore/datasetit/radiotallenteet/"):
	assert os.path.isdir(target_dpath)
	"""TX samples based on input arguments"""
	center_freq = f_center # Hz
	sample_rate = sr # Hz
	num_samps = int(t_total * sample_rate) # number of samples received
	gain = 50 # dB

	usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
	usrp.set_rx_rate(sample_rate, 0)
	usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(center_freq), 0)
	usrp.set_rx_gain(gain, 0)
	print("rx frequency: ", usrp.get_rx_freq())
	print("rx samplerate: ", usrp.get_rx_rate())
	print("rx gain range: ", usrp.get_rx_gain_range())
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
	samples = np.zeros(num_samps, dtype=np.complex128)
	print("Engage recording loop.")
	n_received = 0
	t00 = time.perf_counter()
	while n_received < num_samps:
		nr = streamer.recv(recv_buffer, metadata)
		if (n_received + nr) < num_samps:
			samples[n_received:n_received+nr] = recv_buffer[0,0:nr]
		else:
			samples[n_received:] = recv_buffer[0,0:(num_samps - n_received)]
		n_received += nr
	t11 = time.perf_counter()
	print("Avg emipiric sr: ", np.average( num_samps / (t11-t00) ))
	print("Total time:", t11-t00 )
	# Stop Stream
	stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.stop_cont)
	streamer.issue_stream_cmd(stream_cmd)

	while True:
		n_letters = 1.0
		triplet = "".join( [chr(x) for x in np.random.randint(65,91, int(n_letters))])
		fpath = target_dpath + "uhf-{}_{}MHz-{}ksps.pickled".format(triplet, round(f_center*1e-6,3),  round(1e-3*sr))
		if not os.path.isfile(fpath):
			break
		n_letters += 0.5

	f = open(fpath, "wb")
	f.write(pickle.dumps(samples))
	f.close()
	print("Written into {}".format(fpath))
	return fpath








def show_recording(fpath):
	f = open(fpath, "rb")
	rd = f.read()
	f.close()
	samples = pickle.loads(rd)
	from mtools.tools_dsp import waterfall_mx
	waterfall_mx(samples = samples, fftlen=1024*2, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=True)







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







if __name__ == '__main__':
	#fpath = record(f_center=437.0e6,  sr=1e6, t_total=10.0)
	show_recording(fpaths[6][0])










