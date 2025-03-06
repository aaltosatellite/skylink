import time
import numpy as np
import uhd
import pickle

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






def show_recording():
	nn = 109
	f = open("./recording-{}.dat".format(nn), "rb")
	rd = f.read()
	f.close()
	samples = pickle.loads(rd)
	from mtools.tools_dsp import waterfall_mx
	waterfall_mx(samples = samples, fftlen=1024*2, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=True)




















if __name__ == '__main__':
	#record()
	#show_recording()
	transmit2()

