import uhd
import numpy as np
import threading
from .lib_receiver import ReceiverSettings, Receiver
from .lib_tools import make_samples, ints_to_bits, DEFAULT_SYNCHWORD
from .lib_framing import frame_packet
from .lib_reedsolomon import get_default_rs
from queue import Queue, Empty
import SoapySDR
from SoapySDR import SOAPY_SDR_ABI_VERSION, SOAPY_SDR_RX, SOAPY_SDR_TX, SOAPY_SDR_CF32

DEBUG_PRINT = True

def DBGPRINT(*args, **kwargs):
	if DEBUG_PRINT:
		print(*args, **kwargs)


def get_default_settings(sr, baudrate, f_tune, f_signal):
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

	return settings




class RadioLoop:
	def __init__(self, rx_settings:ReceiverSettings):
		self.preamble_bits = ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
		rs_mx, rs_cfg = get_default_rs()
		self.rs_mx = rs_mx
		self.rs_cfg = rs_cfg
		self.rx_settings = rx_settings
		self.rx = Receiver(settings=self.rx_settings)
		self.que_radio_to_skylink = Queue(100)
		self.que_skylink_to_radio = Queue(3)
		self._internal_sample_que = Queue(250)
		self.receiver_lock = threading.RLock()
		self.on = True
		self.rx_thread 			= threading.Thread(target=None, args=tuple())
		self.rx_process_thread 	= threading.Thread(target=None, args=tuple())
		self.tx_thread 			= threading.Thread(target=None, args=tuple())
		self.exception_counter = 0
		self.warning_vector = [0,0]

	def is_ok(self):
		if not self.on:
			return False
		for thrd in (self.tx_thread, self.rx_process_thread, self.rx_thread):
			if not thrd.is_alive():
				return False
		return True

	def close(self):
		self.on = False
		self.rx_thread.join(timeout=1.0)
		self.rx_process_thread.join(timeout=1.0)
		self.tx_thread.join(timeout=1.0)

	def compose_samples(self, payload):
		pl_char_ints = np.array(bytearray(payload), dtype=np.int64)
		#pl_chars = np.random.randint(0,255, 122)
		bits = frame_packet(pl=pl_char_ints, synchword_int=DEFAULT_SYNCHWORD, synchword_len=32, use_scrambler=True, use_rs=True, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg, nrz_shift=True)
		bits = np.concatenate( (self.preamble_bits, bits) )
		sps = self.rx_settings.sr0 / self.rx_settings.baudrate
		f_center = self.rx.center_frequency_estimate()
		DBGPRINT("-[transmission center freq:  {} MHz]".format( f_center * 1e-6, 4 ))
		f_offset = (f_center - self.rx_settings.f_tune) / self.rx_settings.sr0
		n_silence_start = int(self.rx_settings.sr0 * 5e-3) # TODO: this should be a setting
		samples = make_samples(sps_f=sps, bitstring=bits, f_offset=f_offset, power=1.0,
					 		   mod_index=self.rx_settings.mod_index, shaper_mode=1,
							   shaper_BT_prod=self.rx_settings.BT, shaper_n_taps=int(sps*4)+1,
							   n_silence_start=n_silence_start, n_silence_end=0)
		return samples

	def soapy_start(self):
		# TODO implement soapy version ------------------------------------
		center_freq = self.rx_settings.f_tune # Hz
		sample_rate = self.rx_settings.sr0 # Hz
		args = dict(device="uhd")
		sdr = SoapySDR.Device(args)
		sdr.setSampleRate(SOAPY_SDR_RX, 0, sample_rate)
		sdr.setFrequency(SOAPY_SDR_RX, 0, center_freq)
		print(sdr.getGainRange())
		#txStream = sdr.setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32)
		#sdr.writeStream()
		rxStream = sdr.setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32)
		sdr.activateStream(rxStream) #start streaming
		#create a re-usable buffer for rx samples
		bufferlen = 8*1024
		buff = np.zeros(bufferlen, np.complex64)
		while True:
			ret = sdr.readStream(rxStream, [buff], bufferlen)
			print(ret.ret) #num samples or error code
			print(ret.flags) #flags set by receive operation
			print(ret.timeNs) #timestamp for receive buffer
			break
		sdr.deactivateStream(rxStream) #stop streaming
		sdr.closeStream(rxStream)
		# TODO implement soapy version ------------------------------------

	def usrp_start(self):
		self.rx = Receiver(settings=self.rx_settings)
		# This noise injection enforces the jit-compilation of much of the signal processing pipeline before the loop starts.
		noise = np.random.normal(0,0.1,10000) + np.random.normal(0, 0.1, 10000)*1j
		rx0 = Receiver(settings=self.rx_settings)
		rx0.push_samples(batch=noise)
		center_freq = self.rx_settings.f_tune # Hz
		sample_rate = self.rx_settings.sr0 # Hz
		gain = 50 # dB
		usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
		usrp.set_rx_rate(sample_rate, 0)
		usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(center_freq), 0)
		usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(center_freq), 0)
		usrp.set_rx_gain(gain, 0) # print("rx gain range",usrp.get_rx_gain_range())
		usrp.set_tx_gain(gain, 0) # TODO do this?
		print("[usrp rx-center-f:  {} MHz]".format( round(usrp.get_rx_freq(0)*1e-6, 3) ))
		print("[usrp tx-center-f:  {} MHz]".format( round(usrp.get_tx_freq(0)*1e-6, 3) ))
		self.rx_thread 			= threading.Thread(target=self._usrp_rx_loop,    args=(usrp, 8000), daemon=True) #TODO bufferlen as setting?
		self.rx_process_thread 	= threading.Thread(target=self._rx_process_loop, args=tuple(),      daemon=True)
		self.tx_thread 			= threading.Thread(target=self._usrp_tx_loop,    args=(usrp, 8000), daemon=True) #TODO bufferlen as setting?
		self.on = True
		self.rx_thread.start()
		self.rx_process_thread.start()
		self.tx_thread.start()

	def _usrp_rx_loop(self, usrp:uhd.usrp.MultiUSRP, rx_buffer_len):
		# Set up the stream and receive buffer
		st_args = uhd.usrp.StreamArgs("fc32", "sc16")
		st_args.channels = [0]
		rx_streamer = usrp.get_rx_stream(st_args)
		# Start Stream
		stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
		stream_cmd.stream_now = True
		rx_streamer.issue_stream_cmd(stream_cmd)
		recv_buffer = np.zeros((1, rx_buffer_len), dtype=np.complex64)
		metadata = uhd.types.RXMetadata()
		n_rx_loops = 0
		while self.on:
			try:
				if (n_rx_loops % 1000) == 0:
					DBGPRINT("rx loop #{}".format(n_rx_loops))
				rx_ret = rx_streamer.recv(recv_buffer, metadata)
				assert rx_ret == rx_buffer_len
				if not self._internal_sample_que.full():
					self._internal_sample_que.put_nowait(recv_buffer[0,:])
				elif self.warning_vector[0] == 0:
					self.warning_vector[0] = 1
					raise Warning("radio-loop: radio-to-process queue overflow.")
				n_rx_loops += 1
			except Exception as e:
				print("radio-loop: Exception (rx-radio-rcv-thread)", e)
				self.on = False
				self.exception_counter += 1
				break

	def _rx_process_loop(self):
		while self.on:
			try:
				rx_samples = self._internal_sample_que.get(timeout=0.25)
				with self.receiver_lock:
					rx_pls = self.rx.push_samples(batch=rx_samples, give_bits=False)
				for rx_pl in rx_pls:
					DBGPRINT("+[radio received a payload]")
					if not self.que_radio_to_skylink.full():
						self.que_radio_to_skylink.put_nowait(rx_pl)
					else:
						DBGPRINT("![WARNING: radio-to-skylink queue full]")
						if self.warning_vector[1] == 0:
							self.warning_vector[1] = 1
							raise Warning("radio-loop: process-to-skylink queue overflow.")
			except Empty:
				pass
			except Exception as e:
				print("radio-loop: Exception (rx-process-thread)", e)
				self.on = False
				self.exception_counter += 1
				break

	def _usrp_tx_loop(self, usrp:uhd.usrp.MultiUSRP, tx_batch_len):
		tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
		# stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
		tx_stream_args.channels = [0]
		tx_streamer = usrp.get_tx_stream(tx_stream_args)
		tx_metadata = uhd.types.TXMetadata()
		while self.on:
			try:
				payload = self.que_skylink_to_radio.get(timeout=0.25)
				assert type(payload) == bytes
				samplearr = self.compose_samples(payload)
				DBGPRINT("+[inital samplearr of shape {}]".format(str(samplearr.shape)))
				DBGPRINT("+[inital samplearr of dtype {}]".format(str(samplearr.dtype)))
				assert len(samplearr.shape) == 1
				N = len(samplearr)
				samplearr = np.reshape(samplearr, (1,N))
				samplearr = np.complex64(samplearr)
				idx = 0
				while idx < N:
					tx_streamer.send(samplearr[0,idx:idx+tx_batch_len], tx_metadata)
					idx += tx_batch_len
				DBGPRINT("+[radio transmitted samples]")
			except Empty:
				pass
			except Exception as e:
				print("radio-loop: Exception (tx-thread)", e)
				self.on = False
				self.exception_counter += 1
				break

















