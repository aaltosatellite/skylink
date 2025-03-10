import threading
import time, os
import numpy as np
import uhd
import pickle
from matplotlib import pyplot as plt
from mtools.tools_dsp import waterfall_mx
from scipy.signal import firwin
#Foresail1: 437.125MHz






def transmit():
	from kuokka.lib_tools import make_samples
	#import uhd
	#import numpy as np
	usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
	center_freq_A = 437.1e6
	center_freq1 = 410e6 #437.1e6
	center_freq2 = 420e6 #437.1e6
	center_freq3 = 430e6 #437.1e6
	center_freq4 = 440e6 #437.1e6
	gain = 50

	usrp.set_rx_rate(1e6)
	usrp.set_tx_rate(1e6)
	usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(center_freq1), 0)
	usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(center_freq2), 1)
	usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(center_freq3), 1)
	usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(center_freq4), 0)

	usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(center_freq_A), 0)
	usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(center_freq_A), 1)

	bitstring = np.random.randint(0,2, 8*64) * 2 -1
	txsamples = make_samples(sps_f=1e6/9600, bitstring=bitstring, f_offset=0.125e6/1e6, power=1.0, mod_index=0.5, shaper_mode=1, shaper_BT_prod=-1, shaper_n_taps=301, n_silence_start=100, n_silence_end=10)

	nsamples = len(txsamples)
	assert nsamples > 1000
	txsamples = np.exp(2j*np.pi * np.arange(nsamples) * (1/1e6) * 0.05e6)

	print("rx frequency 0: ", usrp.get_rx_freq(0)/1e6)
	print("rx frequency 1: ", usrp.get_rx_freq(1)/1e6)
	print("tx frequency 0: ", usrp.get_tx_freq(0)/1e6)
	print("tx frequency 1: ", usrp.get_tx_freq(1)/1e6)
	usrp.set_rx_gain(gain, 0)
	print("rx gain range",usrp.get_rx_gain_range())

	rx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
	tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
	#stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
	rx_stream_args.channels = [0]
	tx_stream_args.channels = [0]
	tx_streamer = usrp.get_tx_stream(tx_stream_args)
	tx_metadata = uhd.types.TXMetadata()
	tx_buffer1 = np.zeros((1,int(0.12 * 1e6)), dtype=np.complex64)
	tx_buffer2 = np.zeros((1,int(0.12 * 1e6)), dtype=np.complex64)
	tx_buffer3 = np.zeros((1,int(0.12 * 1e6)), dtype=np.complex64)
	tx_buffer4 = np.zeros((1,int(0.12 * 1e6)), dtype=np.complex64)
	tx_buffer5 = np.zeros((1,int(0.12 * 1e6)), dtype=np.complex64)
	tx_buffer6 = np.zeros((1,int(0.12 * 1e6)), dtype=np.complex64)
	tx_buffer_A = np.reshape(txsamples, (1, len(txsamples)))
	print("----")
	[print(x) for x in tx_streamer.__dir__()]
	print("----")
	print(tx_streamer.get_num_channels())
	print(tx_streamer.get_max_num_samps())
	print(tx_streamer.recv_async_msg())
	print("Sending loop")
	t0 = time.perf_counter()
	while True:
		samps1 = tx_streamer.send(tx_buffer_A, tx_metadata)
		print(time.perf_counter() - t0)
		t0 = time.perf_counter()

		samps2 = tx_streamer.send(tx_buffer_A, tx_metadata)
		print(time.perf_counter() - t0)
		t0 = time.perf_counter()

		samps3 = tx_streamer.send(tx_buffer_A, tx_metadata)
		print(time.perf_counter() - t0)
		t0 = time.perf_counter()


		print("samps ret: ", samps1, samps2, samps3)
		time.sleep(1.0)
		#break












def transmit2():
	import threading
	from kuokka.lib_tools import make_samples
	#import uhd
	#import numpy as np
	usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
	center_freq_A = 437.1e6
	gain = 70

	usrp.set_rx_rate(1e6)
	usrp.set_tx_rate(1e6)
	usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(center_freq_A), 0)
	usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(center_freq_A), 0)

	bitstring = np.random.randint(0,2, 8*64) * 2 -1
	txsamples = make_samples(sps_f=1e6/9600, bitstring=bitstring, f_offset=0.125e6/1e6, power=1.0, mod_index=0.5, shaper_mode=1, shaper_BT_prod=-1, shaper_n_taps=301, n_silence_start=100, n_silence_end=10)
	nsamples = len(txsamples)
	assert nsamples > 1000
	txsamples = np.exp(2j*np.pi * np.arange(nsamples*2) * (1/1e6) * 0.05e6)
	#txsamples = np.random.normal(0,1, nsamples*2) + np.random.normal(0,1, nsamples*2)*1j
	txsamples = np.complex64(np.ones( int(1.0*1e6) ))

	print("rx frequency 0: ", usrp.get_rx_freq(0)/1e6)
	print("tx frequency 0: ", usrp.get_tx_freq(0)/1e6)
	usrp.set_rx_gain(gain, 0)
	usrp.set_tx_gain(gain, 0)
	print("rx gain range",usrp.get_rx_gain_range())

	#rx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
	#rx_stream_args.channels = [0]
	#rx_streamer = usrp.get_rx_stream(rx_stream_args)
	#rx_thrd = threading.Thread(target=rx_function, args=(rx_streamer,))
	#rx_thrd.start()


	tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
	#stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
	tx_stream_args.channels = [0]
	tx_streamer = usrp.get_tx_stream(tx_stream_args)
	tx_metadata = uhd.types.TXMetadata()
	tx_buffer_A = np.reshape(txsamples, (1, len(txsamples)))
	tx_buffer_A = np.complex64(tx_buffer_A)
	print("---------------------------")
	[print(x) for x in tx_streamer.__dir__()]
	print(tx_streamer.get_num_channels())
	print(tx_streamer.get_max_num_samps())
	print(tx_streamer.recv_async_msg())
	print("---------------------------")
	print("Sending loop")
	while True:
		t0 = time.perf_counter()
		samps1 = tx_streamer.send(tx_buffer_A, tx_metadata)
		print("tx-dt: ", round(time.perf_counter() - t0, 3))
		print("samps ret: ", samps1)
		time.sleep(0.5)










def make_rising_waves(nsamples, rel_offset, power_levels):
	samples = np.exp(2j*np.pi * np.arange(nsamples) * rel_offset)
	n_per_level = nsamples // len(power_levels)
	for i in range(len(power_levels)):
		p = power_levels[i]
		in0 = n_per_level*i
		in1 = n_per_level*(i+1)
		samples[in0:in1] = samples[in0:in1] * p #np.linspace(0, p, n_per_level)
	return samples




def tx_loop(usrp, tx_samples, start_wait):
	tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
	#stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
	tx_stream_args.channels = [1]
	tx_streamer = usrp.get_tx_stream(tx_stream_args)
	tx_metadata = uhd.types.TXMetadata()

	assert len(tx_samples.shape) == 1
	nsamples = len(tx_samples)
	tx_samples = np.reshape(tx_samples, (1, len(tx_samples)))

	print("---------------------------")
	[print(x) for x in tx_streamer.__dir__()]
	print("get_num_channels()", tx_streamer.get_num_channels())
	print("get_max_num_samps()", tx_streamer.get_max_num_samps())
	print("recv_async_msg()", tx_streamer.recv_async_msg())
	print("---------------------------")
	time.sleep(start_wait)
	print("Tramsitting...")
	c = 0
	while True:
		if c > nsamples - 2048*2:
			c = 0
			time.sleep(start_wait)
			break
		tx_streamer.send(tx_samples[:,c:c+2048*2], tx_metadata)
		c += 2048*2
	tx_metadata.end_of_burst = True
	tx_streamer.send(tx_samples[:,0:10], tx_metadata)




def self_record(f_center, sr, t_total, target_dpath = "/home/elmore/datasetit/radiotallenteet/"):
	assert os.path.isdir(target_dpath)
	"""TX samples based on input arguments"""
	center_freq = f_center # Hz
	sample_rate = sr # Hz
	num_samps = (1+int(t_total*sample_rate/2048))*2048 # number of samples received
	gain = 70 # dB

	usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
	usrp.set_rx_rate(sample_rate, 0)
	usrp.set_tx_rate(sample_rate, 0)

	print("Frequencies==========================")
	print("Setting RX 0 to 100M,    1 to 150M")
	print("Setting TX 0 to 200M,    1 to 250M")

	usrp.set_rx_freq(100e6, 0)
	#usrp.set_rx_antenna("TX/RX", 0)
	usrp.set_rx_freq(150e6, 1)
	usrp.set_tx_freq(200e6, 0)
	usrp.set_tx_freq(250e6, 1)
	print("RX-F-0: ", usrp.get_rx_freq(0))
	print("RX-F-1: ", usrp.get_rx_freq(1))
	print("TX-F-0: ", usrp.get_tx_freq(0))
	print("TX-F-1: ", usrp.get_tx_freq(1))
	print("RX, TX Antennas 0:",  usrp.get_rx_antenna(0), usrp.get_tx_antenna(0))
	print("RX, TX Antennas 1:",  usrp.get_rx_antenna(1), usrp.get_tx_antenna(1))



	print("================================================")
	#time.sleep(10)
	#import sys
	#sys.exit(0)

	usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(center_freq+11.99e6), 0)

	print("RX BW range0: ", usrp.get_rx_bandwidth_range(0))
	print("RX BW range1: ", usrp.get_rx_bandwidth_range(1))
	print("TX BW range0: ", usrp.get_tx_bandwidth_range(0))
	print("TX BW range1: ", usrp.get_tx_bandwidth_range(1))
	usrp.set_rx_bandwidth(sr*2.00,0)
	print("RX BW 0: ", usrp.get_rx_bandwidth(0))
	usrp.set_rx_gain(gain, 0) # 0, 76,    1
	usrp.set_tx_gain(gain, 0) # 0, 89.75, 0.25
	print("rx antennas 0: ", usrp.get_rx_antennas(0))
	print("rx antennas 1: ", usrp.get_rx_antennas(1))
	print("tx antennas 0: ", usrp.get_tx_antennas(0))
	print("tx antennas 1: ", usrp.get_tx_antennas(1))
	print("rx antenna: ", usrp.get_rx_antenna(0))
	print("tx antenna: ", usrp.get_tx_antenna(0))
	#usrp.set_rx_antenna()
	#usrp.set_rx_antenna()

	print("rx frequency 0: ", usrp.get_rx_freq(0))
	print("rx frequency 1: ", usrp.get_rx_freq(1))
	print("rx samplerate: ", usrp.get_rx_rate())
	print("rx gain range: ", usrp.get_rx_gain_range())
	print("tx gain range: ", usrp.get_tx_gain_range())
	print("")
	print("rx antennas: ", usrp.get_rx_antennas())
	print("rx num channels: ", usrp.get_rx_num_channels())
	print("tx antennas: ", usrp.get_tx_antennas())
	print("tx num channels: ", usrp.get_tx_num_channels())

	# Set up the RX stream and buffer
	rx_st_args = uhd.usrp.StreamArgs("fc32", "sc16")
	rx_st_args.channels = [0]
	print("rx_st_args", rx_st_args.args)
	print("rx_st_args", rx_st_args.otw_format)
	print("rx_st_args", rx_st_args.cpu_format)
	metadata = uhd.types.RXMetadata()
	rx_streamer = usrp.get_rx_stream(rx_st_args)
	rx_samples = np.zeros(num_samps, dtype=np.complex64)
	rx_buffer = np.zeros((1, 1024*4), dtype=np.complex64)

	tx_buffer = make_rising_waves( nsamples=int(t_total*0.7*sr), rel_offset=0.1, power_levels=(0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 10.8) )
	tx_buffer = np.array(tx_buffer, dtype=np.complex64)
	tx_thread = threading.Thread(target=tx_loop, args=(usrp, tx_buffer, 0.5))
	#tx_thread.start()

	# Start RX Stream
	rx_stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
	rx_stream_cmd.stream_now = True
	rx_streamer.issue_stream_cmd(rx_stream_cmd)

	# Receive Samples
	print("Engage recording loop.")
	n_received = 0
	t00 = time.perf_counter()
	while n_received < num_samps:
		nr = rx_streamer.recv(rx_buffer, metadata)
		if (n_received + nr) < num_samps:
			rx_samples[n_received:n_received+nr] = rx_buffer[0,0:nr]
		else:
			rx_samples[n_received:] = rx_buffer[0,0:(num_samps - n_received)]
		n_received += nr
	t11 = time.perf_counter()
	print("Avg emipiric sr: ", np.average( num_samps / (t11-t00) ))
	print("Total time:", t11-t00 )

	# Stop Stream
	stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.stop_cont)
	rx_streamer.issue_stream_cmd(stream_cmd)

	while True:
		n_letters = 1.0
		triplet = "".join( [chr(x) for x in np.random.randint(65,91, int(n_letters))])
		fpath = target_dpath + "self_record-{}_{}MHz-{}ksps.pickled".format(triplet, round(f_center*1e-6,3),  round(1e-3*sr))
		if not os.path.isfile(fpath):
			break
		n_letters += 0.5

	f = open(fpath, "wb")
	f.write(pickle.dumps(np.array(rx_samples, dtype=np.complex128)))
	f.close()
	print("Written into {}".format(fpath))
	return fpath












def show_recording(fpath):
	assert os.path.isfile(fpath)
	f = open(fpath, "rb")
	rd = f.read()
	f.close()
	samples = pickle.loads(rd)

	waterfall_mx(samples = samples, fftlen=1024*2, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=True)











if __name__ == '__main__':
	#record()
	#show_recording()
	#transmit2()

	fpath = self_record(f_center=384e6, sr=4e6, t_total=2.0, target_dpath="/home/elmore/datasetit/radiotallenteet/")

	#fpath = "/home/elmore/datasetit/radiotallenteet/self_record-A_150.0MHz-1000ksps.pickled"

	#show_recording(fpath)

	f = open(fpath, "rb")
	rd = f.read()
	f.close()

	samples = pickle.loads(rd)
	samples = np.array(samples, dtype=np.complex128)
	#samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) * (1/1e6) * -100e3)

	#lp_taps = firwin(numtaps=201, cutoff=0.4, fs=1.0, pass_zero=True)
	#lp_samples = np.convolve(samples, lp_taps)

	waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, srate=4e6, plot_and_show=True, y_is_time=True)

	fig = plt.figure(figsize=(14,14))
	ax1 = fig.add_subplot(211)
	ax2 = fig.add_subplot(212)

	#ax1.plot(np.arange(len(lp_samples)),  np.abs(lp_samples))
	#ax1.grid()

	k = np.zeros(2048)
	freqs = np.fft.fftshift(np.fft.fftfreq(2048, 1/4e6))
	for _ in range(24):
		i = np.random.randint(0, len(samples)-2048*2)
		k += np.abs(np.fft.fftshift(np.fft.fft(samples[i:i+2048])))
	ax2.plot(freqs, k)
	ax2.grid()

	fig.set_layout_engine("tight")
	plt.show()




























