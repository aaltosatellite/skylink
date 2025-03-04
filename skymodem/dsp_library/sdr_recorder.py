import uhd
import os
import time
import numpy as np
import pickle


def record(f_center, sr, t_total):
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

	rint = np.random.randint(0,1000)
	fpath = "/home/elmore/datasetit/radiotallenteet/uhf-{}_{}MHz-{}ksps.pickled".format(rint, round(f_center*1e-6,2),  round(1e-3*sr))
	f = open(fpath, "wb")
	f.write(pickle.dumps(samples))
	f.close()
	print("Written into {}".format(fpath))
	return rint





def show_recording(nn):
	#nn = 50
	#nn = 583
	#nn = 468
	fpath = "/home/elmore/datasetit/radiotallenteet/uhf-{}_437.0MHz-1000ksps.pickled".format(nn)
	f = open(fpath, "rb")
	rd = f.read()
	f.close()
	samples = pickle.loads(rd)
	from mtools.tools_dsp import waterfall_mx
	waterfall_mx(samples = samples, fftlen=1024*2, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=True)








if __name__ == '__main__':
	rint = record(f_center=437.0e6,  sr=1e6, t_total=9.0)

	#/home/elmore/datasetit/radiotallenteet/uhf-969_437.0MHz-1000ksps.pickled
	show_recording(rint)










