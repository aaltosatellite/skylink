import uhd
import numpy as np
import time
from kuokka.lib_receiver import Receiver, ReceiverSettings


def get_receiver(sr, baudrate, f_tune, f_signal):
	#baudrate			= 9600			# tx param
	sps  				= 21			# todo measure final A against a spectrum of sps's....
	mod_index			= 0.5			# tx param
	batch_maxlen 		= 6000
	#f_tune				= 437.1e6
	#f_signal			= 437.00e6 + 125e3

	settings = ReceiverSettings(sr0=sr, baudrate=baudrate, bufferlen=600000, batch_maxlen=batch_maxlen, f_tune=f_tune, f_expected=f_signal)
	settings.sps 					= sps
	settings.baudrate 				= baudrate
	settings.lp_cutoff_coeff 		= 0.625 #0.625
	settings.lp_ntaps				= 161
	settings.JPL_n_decay 			= 28.0
	settings.synch_delay_mpr 		= 22.0

	settings.rs_f_cutoff_coeff		= 0.499
	settings.m_halflen				= 25

	settings.fftlen					= 1024
	settings.jumplen				= 1024//2
	settings.mod_index				= mod_index
	settings.mask_mode				= 1
	settings.c_stat_update			= 1 / 700
	settings.c_f_update_minimum 	= 0.02
	settings.T_f_upd_recovery 		= 2.5
	settings.fft_trigger_on_level 	= 4.9
	settings.fft_trigger_off_level 	= 3.0
	settings.start_margin_mpr		= 1.0
	settings.end_margin_mpr			= 0.4

	rx = Receiver(settings=settings)
	return rx, settings



def record():
	"""TX samples based on input arguments"""
	usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")

	center_freq = 437.100e6 # Hz
	sample_rate = 1e6 # Hz
	gain = 50 # dB

	rx, settings = get_receiver(sr=sample_rate, baudrate=9600, f_tune=center_freq, f_signal=437.125e6)

	usrp.set_rx_rate(sample_rate, 0)
	usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(center_freq), 0)
	usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(center_freq), 0)
	usrp.set_rx_gain(gain, 0)
	print("rx gain range",usrp.get_rx_gain_range())
	print("rx frequency: ", usrp.get_rx_freq())
	print("tx frequency: ", usrp.get_tx_freq())


	# Set up the stream and receive buffer
	st_args = uhd.usrp.StreamArgs("fc32", "sc16")
	st_args.channels = [0]
	metadata = uhd.types.RXMetadata()

	rx_streamer = usrp.get_rx_stream(st_args)


	print("tx gain range:",usrp.get_tx_gain_range(0))
	print("tx gain range:",usrp.get_tx_gain_range())
	print("rx gain range:",usrp.get_rx_gain_range(0))
	print("rx gain range:",usrp.get_rx_gain_range())

	print("tx antennas: ",usrp.get_tx_antennas())
	print("rx antennas: ",usrp.get_rx_antennas())


	# Start Stream
	stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
	stream_cmd.stream_now = True
	rx_streamer.issue_stream_cmd(stream_cmd)

	# Receive Samples
	recv_buffer = np.zeros((1, 8000), dtype=np.complex64)
	avg_speed = 0.0
	payloads = list()
	ii = 0
	[print(x) for x in rx_streamer.__dir__()]
	print(rx_streamer.get_max_num_samps())
	print("Engage receiving loop.")
	t00 = time.perf_counter()
	t0 = time.perf_counter()
	rx_total = 0
	while True:
		rx_ret = rx_streamer.recv(recv_buffer, metadata)
		rx_pls = rx.push_samples(batch=recv_buffer[0,:], give_bits=False)
		rx_total += rx_ret
		if rx_pls:
			payloads.extend(rx_pls)
			print("received {} payloads.".format(len(rx_pls)))
			for pl in rx_pls:
				print("\t", pl)
		if (ii%500) == 0:
			print("rx_ret: ", rx_ret)
			print("avg speed: ", avg_speed)
			print("")

		t1 = time.perf_counter()
		dt = t1 - t0
		t0 = t1
		avg_speed = rx_total / (time.perf_counter() - t00)
		ii += 1

	# Stop Stream
	#stream_cmd_stop = uhd.types.StreamCMD(uhd.types.StreamMode.stop_cont)
	#rx_streamer.issue_stream_cmd(stream_cmd_stop)


record()