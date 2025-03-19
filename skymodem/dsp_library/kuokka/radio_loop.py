import time
import uhd
import numpy as np
import threading
from .lib_receiver import ReceiverConfig, Receiver, precompile_receiver
from .lib_tools import make_samples, ints_to_bits, DEFAULT_SYNCHWORD, DEFAULT_SYNCHWORD_LEN, doppler_correction
from .lib_framing import frame_packet
from .lib_reedsolomon import get_default_rs
from queue import Queue, Empty
import SoapySDR
from SoapySDR import SOAPY_SDR_ABI_VERSION, SOAPY_SDR_RX, SOAPY_SDR_TX, SOAPY_SDR_CF32
from datetime import datetime as dtime

DEBUG_PRINT_ON = True
def DBGPRINT(*args, **kwargs):
	ts = "[{}]".format( dtime.now().isoformat()[-15:] )
	ts += " "*(17-len(ts)) + "[RadioLoop]" + " "
	first, args = args[0], args[1:]
	if DEBUG_PRINT_ON:
		print(ts+str(first), *args, **kwargs)




class RadioLoop:
	def __init__(self, rx_config:ReceiverConfig):
		DBGPRINT("Precompile DSP")
		precompile_receiver(rx_config, do_print=False)
		self.rx_config 				= rx_config
		self.frequency_following 		= True
		self.use_doppler_correction 	= True
		self.preamble_bits = ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
		rs_mx, rs_cfg = get_default_rs()
		self.rs_mx = rs_mx
		self.rs_cfg = rs_cfg
		self.rx = Receiver(config=self.rx_config)
		self.que_radio_to_skylink = Queue(64)
		self.que_skylink_to_radio = Queue(3)
		self._internal_sample_que = Queue(256)
		self.receiver_lock = threading.RLock()
		self.on = True
		self.rx_thread 			= threading.Thread(target=None, args=tuple())
		self.rx_process_thread 	= threading.Thread(target=None, args=tuple())
		self.tx_thread 			= threading.Thread(target=None, args=tuple())
		self.self_mute = False
		self.last_verified_freq = (0, 0.0)
		self.own_recently_sent = dict()

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

	def soapy_start(self):
		# TODO implement soapy version ------------------------------------
		center_freq = self.rx_config.f_tune # Hz
		sample_rate = self.rx_config.sr0 # Hz
		args = dict(device="uhd")
		sdr = SoapySDR.Device(args)
		sdr.setSampleRate(SOAPY_SDR_RX, 0, sample_rate)
		sdr.setFrequency(SOAPY_SDR_RX, 0, center_freq)
		DBGPRINT("Gain Range: {}".format( sdr.getGainRange()))
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
		# This noise injection enforces the jit-compilation of much of the signal processing pipeline before the loop starts.
		gain = 56 # dB
		usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
		usrp.set_rx_rate(self.rx_config.sr0, 0)
		usrp.set_tx_rate(self.rx_config.sr0, 0)
		usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(self.rx_config.f_tune), 0)
		usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(self.rx_config.f_tune), 0)
		usrp.set_rx_gain(gain, 0)
		usrp.set_tx_gain(gain, 0)
		DBGPRINT("RX gain range:      {}".format( str(usrp.get_rx_gain_range(0))[:-1] ))
		DBGPRINT("TX gain range:      {}".format( str(usrp.get_tx_gain_range(0))[:-1] ))
		DBGPRINT("usrp RX gain:       {}".format( usrp.get_rx_gain(0) ))
		DBGPRINT("usrp TX gain:       {}".format( usrp.get_tx_gain(0) ))
		DBGPRINT("usrp RX samplerate: {} ksps".format( round(usrp.get_rx_rate(0)*1e-3, 3) ))
		DBGPRINT("usrp TX samplerate: {} ksps".format( round(usrp.get_tx_rate(0)*1e-3, 3) ))
		DBGPRINT("usrp RX tune-f:     {} MHz".format( round(usrp.get_rx_freq(0)*1e-6, 3) ))
		DBGPRINT("usrp TX tune-f:     {} MHz".format( round(usrp.get_tx_freq(0)*1e-6, 3) ))
		self.rx 				= Receiver(config=self.rx_config)
		#self.rx_thread 		= threading.Thread(target=self._recording_rx_loop,    args=(1024*4,), daemon=True) #TODO bufferlen as setting?
		self.rx_thread 			= threading.Thread(target=self._usrp_rx_loop,    args=(usrp, 1024*4), daemon=True) #TODO bufferlen as setting?
		self.rx_process_thread 	= threading.Thread(target=self._rx_process_loop, args=tuple(),      daemon=True)
		self.tx_thread 			= threading.Thread(target=self._usrp_tx_loop,    args=(usrp, 1024*4), daemon=True) #TODO bufferlen as setting?
		self.on = True
		self.rx_thread.start()
		self.rx_process_thread.start()
		self.tx_thread.start()

	def get_state(self):
		with self.receiver_lock:
			d_state = dict()
			d_state["baudrate"] = self.rx_config.baudrate
			d_state["frequency-following-on"] = self.frequency_following
			d_state["doppler-correction-on"] = self.use_doppler_correction
			d_state["f-sdr-tune"] = self.rx_config.f_tune
			d_state["f-center"] = self.rx_config.f_center
			if self.last_verified_freq[1] == 0.0:
				d_state["f-last-reception"] = None
			else:
				d_state["f-last-reception"] = self.last_verified_freq[0], time.monotonic() - self.last_verified_freq[1]
			return d_state

	def set_doppler_correction(self, toggle:bool):
		with self.receiver_lock:
			self.use_doppler_correction = bool(toggle)

	def set_baudrate(self, baudrate:int):
		with self.receiver_lock:
			self.rx.switch_baudrate(baudrate=baudrate, sps=self.rx.config.sps)
			self.rx_config.baudrate = baudrate



	# == static functions ==================================================================================================================================================
	# ======================================================================================================================================================================
	def _clean_own_sent(self):
		for key in list(self.own_recently_sent):
			if (time.monotonic() - self.own_recently_sent[key]) > 3.0:
				del self.own_recently_sent[key]


	def _get_transmit_frequency(self, as_offset:bool):
		if self.frequency_following and ((time.monotonic() - self.last_verified_freq[1]) < 60.0) and (self.last_verified_freq[1] > 0):
			f_offset = self.last_verified_freq[0]
			f_recv_abs = f_offset + self.rx_config.f_tune
			if self.use_doppler_correction:
				f_use_abs, _ = doppler_correction(f_received=f_recv_abs, f_original=self.rx_config.f_center, f_at_target=self.rx_config.f_center)
			else:
				f_use_abs = f_recv_abs
		else:
			f_use_abs = self.rx_config.f_center
		if as_offset:
			return f_use_abs - self.rx_config.f_tune
		return f_use_abs


	def _compose_samples(self, payload, usrp_reshape, as_c64):
		f_use_offset = self._get_transmit_frequency(as_offset=True)
		f_offset_nrm = f_use_offset / self.rx_config.sr0
		pl_char_ints = np.array(bytearray(payload), dtype=np.int64)
		bits = frame_packet(pl=pl_char_ints, synchword_int=DEFAULT_SYNCHWORD, synchword_len=DEFAULT_SYNCHWORD_LEN, use_scrambler=True, use_rs=True, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg, nrz_shift=True)
		bits = np.concatenate( (self.preamble_bits, bits) )
		sps = self.rx_config.sr0 / self.rx_config.baudrate
		n_silence_start = int(self.rx_config.sr0 * 5e-3) # TODO: this should be a setting
		samples = make_samples(sps_f=sps, bitstring=bits, f_offset=f_offset_nrm, power=1.0, mod_index=self.rx_config.mod_index, shaper_mode=1,
							   shaper_BT_prod=self.rx_config.BT, shaper_n_taps=int(sps * 4) + 1, n_silence_start=n_silence_start, n_silence_end=0)
		if usrp_reshape:
			samples = np.reshape(samples, (1, len(samples)))
		if as_c64:
			samples = np.array(samples, dtype=np.complex64)
		return samples, f_use_offset+self.rx_config.f_tune


	def _recording_rx_loop(self, rx_buffer_len):
		import pickle
		#fpath7 = "/home/elmore/datasetit/radiotallenteet/uhf-298_437.0MHz-1000ksps.pickled"
		#fpath8 = "/home/elmore/datasetit/radiotallenteet/uhf-447_437.0MHz-1000ksps.pickled"
		#fpath9 = "/home/elmore/datasetit/radiotallenteet/uhf-195_437.0MHz-1000ksps.pickled"
		fpath, fcenter0 = ("/home/elmore/datasetit/radiotallenteet/uhf-965_437.0MHz-1000ksps.pickled",-124.0e3)
		f = open(fpath, "rb")
		rd = f.read()
		f.close()
		samples = pickle.loads(rd)
		samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) * (1/1e6) * (fcenter0+25e3))
		assert len(samples.shape) == 1
		assert type(samples) == np.ndarray
		samples = np.complex64(samples)
		time.sleep(4)
		cursor = 0
		n_received = 0
		t0 = time.perf_counter()
		t_sleep = 0.0
		while self.on:
			time.sleep(t_sleep)
			batch = samples[cursor:cursor+rx_buffer_len]
			assert len(batch) == rx_buffer_len
			cursor += rx_buffer_len
			if cursor > (len(samples) - rx_buffer_len):
				cursor = 0
				DBGPRINT("Recordning cursor zeroed.")
			if not self._internal_sample_que.full():
				self._internal_sample_que.put_nowait(batch)
			else:
				DBGPRINT("WARNING! radio-to-process queue overflow!  {}".format( 1e-6 * n_received / (time.perf_counter() - t0) ))
			n_received += rx_buffer_len
			t_next = t0 + ((n_received + rx_buffer_len) / self.rx_config.sr0)
			t_sleep = max(0, t_next - time.perf_counter())


	def _usrp_rx_loop(self, usrp:uhd.usrp.MultiUSRP, rx_buffer_len):
		# Set up the stream and receive buffer
		st_args = uhd.usrp.StreamArgs("fc32", "sc16")
		st_args.channels = [0]
		rx_streamer = usrp.get_rx_stream(st_args)
		# Start Stream
		stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
		stream_cmd.stream_now = True
		recv_buffer = np.zeros((1, rx_buffer_len), dtype=np.complex64)
		metadata = uhd.types.RXMetadata()
		n_rx_loops = 0
		rx_streamer.issue_stream_cmd(stream_cmd)
		while self.on:
			if (n_rx_loops % 1000) == 0:
				DBGPRINT("(rx-#{})".format(n_rx_loops))
			rx_ret = rx_streamer.recv(recv_buffer, metadata) #blocking until rx_buffer_len samples acquired
			assert rx_ret == rx_buffer_len
			#if self.self_mute:
			#	continue
			if not self._internal_sample_que.full():
				self._internal_sample_que.put_nowait(recv_buffer[0,:].copy())
			else:
				DBGPRINT("WARNING: radio-to-process queue overflow!")
				raise Exception("radio-loop: radio-to-process queue overflow.")
			n_rx_loops += 1


	def _rx_process_loop(self):
		while self.on:
			try:
				rx_samples = self._internal_sample_que.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Queue.get() exception in rx_process_loop: ", e)
				self.on = False
				break
			with self.receiver_lock:
				rx_pls = self.rx.push_samples(batch=rx_samples, give_bits=False)
				for rx_pl, rx_pl_f_offset in rx_pls:
					if rx_pl in self.own_recently_sent:
						self._clean_own_sent()
						#del self.own_recently_sent[rx_pl]
						DBGPRINT("Discarded a self-reception.")
						continue
					DBGPRINT("Radio decoded a frame at {} MHz: \n\033[92m{}\033[0m\n".format(round( (self.rx_config.f_tune+rx_pl_f_offset)*1e-6, 4), rx_pl ))
					self.last_verified_freq = rx_pl_f_offset, time.monotonic()
					if not self.que_radio_to_skylink.full():
						self.que_radio_to_skylink.put_nowait(rx_pl)
					else:
						DBGPRINT("WARNING: radio-to-skylink queue full")
						raise Exception("process-to-skylink queue overflow")


	def _usrp_tx_loop(self, usrp:uhd.usrp.MultiUSRP, tx_batch_len):
		tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
		# stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
		tx_stream_args.channels = [0]
		tx_streamer = usrp.get_tx_stream(tx_stream_args)
		tx_metadata = uhd.types.TXMetadata()
		while self.on:
			try:
				payload = self.que_skylink_to_radio.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Queue.get() exception (tx-thread):", e)
				self.on = False
				break
			with self.receiver_lock:
				self._clean_own_sent()
				self.own_recently_sent[payload] = time.monotonic()
				samplearr, f_use_abs = self._compose_samples(payload, usrp_reshape=True, as_c64=True)
				N = samplearr.shape[1]
				dtt = N / self.rx_config.sr0
			DBGPRINT("tx start at {} MHz".format( f_use_abs * 1e-6, 4 ))
			idx = 0
			t_end = time.perf_counter() + dtt
			self.self_mute = True  # the 5ms initial silence in composed samples also ensures this will have effect.
			while idx < N:
				if (N - idx) <= tx_batch_len:
					tx_metadata.end_of_burst = True
				tx_streamer.send(samplearr[0,idx:idx+tx_batch_len], tx_metadata)
				idx += tx_batch_len
			tx_metadata.end_of_burst = False
			t_to_end = max(0, t_end - time.perf_counter())
			time.sleep(t_to_end + 0.0e-3)
			self.self_mute = False
			DBGPRINT("tx end. sleep of {}/{} ms.".format( round(t_to_end*1e3, 2), round(dtt*1e3, 2) ))

















