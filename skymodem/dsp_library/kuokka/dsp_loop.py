import queue
import numpy as np
from .lib_receiver import Receiver, RXDSPConfig, precompile_receiver
from .lib_framing import frame_packet
from .lib_tools import doppler_correction, ints_to_bits, DEFAULT_SYNCHWORD_LEN, DEFAULT_SYNCHWORD, make_samples2, snr_dB
from .lib_reedsolomon import get_default_rs
from datetime import datetime as dtime
import threading
from queue import Queue, Empty
import time
import multiprocessing as mpr
from multiprocessing import shared_memory



DEBUG_PRINT_ON = True
def DBGPRINT(*args, **kwargs):
	ts = "[{}]".format( dtime.now().isoformat()[-15:] )
	ts += " "*(17-len(ts)) + "[DSPLoop]" + "   "
	first, args = args[0], args[1:]
	if DEBUG_PRINT_ON:
		print(ts+str(first), *args, **kwargs)



class TXDSPConfig:
	def __init__(self, tx_sr0, tx_f_tune, tx_f_center, baudrate):
		# radio device -------------------------------------
		self.tx_sr0				= tx_sr0
		self.tx_f_tune			= tx_f_tune
		# --------------------------------------------------
		# signal properties --------------------------------
		self.tx_f_center 		= tx_f_center
		self.baudrate			= baudrate		# Baudrate of the transmission. Has a definite effect on performance. More so if resampling rate is not adjusted.
		# --------------------------------------------------
		# --------------------------------------------------
		self.tx_BT				= 0.5
		self.tx_mod_index		= 0.5
		# --------------------------------------------------
		self.tx_f_adjustment_halfband = 12e3

	def check_validity(self):
		assert 1e3 < self.tx_sr0 < 32e6
		assert self.tx_f_tune > 1.0e3
		assert self.tx_f_center > 1.0e3
		assert 0 < self.baudrate < (self.tx_sr0/2)
		assert (abs(self.tx_f_tune - self.tx_f_center) + self.tx_f_adjustment_halfband + self.baudrate * 0.6) < (0.5 * self.tx_sr0), "Radio tuned to this frequency with this samplerate cannot see the entire band."
		assert (self.tx_BT >= 0.4) or (self.tx_BT == -1)
		assert 0.5 <= self.tx_mod_index < 10.0
		if not (self.tx_mod_index in (0.5, 0.75)):
			raise Warning("Non standard transmission modulation index of", self.tx_mod_index)







class DSPLoop:
	def __init__(self, rx_dsp_config:RXDSPConfig, tx_dsp_config:TXDSPConfig, que_rx_samples_in:Queue, que_rx_payloads_out:queue.Queue, que_tx_payloads_in:Queue, que_tx_samples_out:Queue, que_signaldata_out:Queue):
		DBGPRINT("Precompile DSP")
		precompile_receiver(rx_dsp_config, do_print=False)
		self.rx_dsp_config 			= rx_dsp_config
		self.tx_dsp_config			= tx_dsp_config
		self.frequency_following 	= True
		self.baudrate_following 	= True
		self.use_doppler_correction = False
		self.preamble_bits 			= ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
		rs_mx, rs_cfg 				= get_default_rs()
		self.rs_mx 					= rs_mx
		self.rs_cfg 				= rs_cfg
		self.rlock 					= threading.RLock()
		self.que_rx_samples_in		= que_rx_samples_in
		self.que_rcv_payloads_out	= que_rx_payloads_out
		self.que_tx_payloads_in		= que_tx_payloads_in
		self.que_tx_samples_out		= que_tx_samples_out
		self.que_signaldata_out 	= que_signaldata_out
		self.rx 					= Receiver(config=rx_dsp_config)
		self.rx_process_thread 		= threading.Thread(target=None, args=tuple())
		self.tx_process_thread 		= threading.Thread(target=None, args=tuple())
		self.last_verified_freq 	= (0, 0.0)  # (absolute_frequency, monotonic_timestamp)
		self.last_verified_baudrate = None
		self.own_recently_sent 		= dict()
		self.t_projected_tx_end		= time.monotonic()
		self.on 					= True


	def is_ok(self):
		if not self.on:
			return False
		if not self.rx_process_thread.is_alive():
				return False
		if not self.tx_process_thread.is_alive():
				return False
		return True


	def close(self):
		self.on = False
		self.rx_process_thread.join(timeout=1.0)
		self.tx_process_thread.join(timeout=1.0)


	def start(self):
		self.rx_process_thread = threading.Thread(target=self._rx_loop, args=tuple(), daemon=True)
		self.rx_process_thread.start()
		self.tx_process_thread = threading.Thread(target=self._tx_loop, args=tuple(), daemon=True)
		self.tx_process_thread.start()


	def set_baudrate(self, baudrate):
		with self.rlock:
			self.tx_dsp_config.baudrate = baudrate
			self.rx_dsp_config.baudrate = baudrate
			self.rx.switch_baudrate(baudrate, sps=self.rx.config.sps)


	def set_doppler_correction(self, toggle:bool):
		with self.rlock:
			self.use_doppler_correction = bool(toggle)


	def get_state(self):
		with self.rlock:
			d_state = dict()
			d_state["baudrate"] = self.rx_dsp_config.baudrate
			d_state["frequency-following-on"] = self.frequency_following
			d_state["doppler-correction-on"] = self.use_doppler_correction
			d_state["f-sdr-tune"] = self.rx_dsp_config.rx_f_tune
			d_state["f-center"] = self.rx_dsp_config.rx_f_center
			if self.last_verified_freq[1] == 0.0:
				d_state["f-last-reception"] = None
			else:
				d_state["f-last-reception"] = self.last_verified_freq[0], time.monotonic() - self.last_verified_freq[1]
			return d_state



	# == private functions ===================================================================================================================================================================
	# ========================================================================================================================================================================================
	def _clean_own_sent(self):
		for key in list(self.own_recently_sent):
			if (time.monotonic() - self.own_recently_sent[key]) > 3.0:
				del self.own_recently_sent[key]


	def _get_transmit_frequency(self, as_offset:bool):
		if self.frequency_following and ((time.monotonic() - self.last_verified_freq[1]) < 60.0) and (self.last_verified_freq[1] > 0):
			f_recv_abs = self.last_verified_freq[0]
			if self.use_doppler_correction:
				f_use_abs, _ = doppler_correction(f_rx_received=f_recv_abs, f_rx_original=self.rx_dsp_config.rx_f_center, f_tx_at_target=self.tx_dsp_config.tx_f_center)
			else:
				f_use_abs = f_recv_abs
		else:
			f_use_abs = self.tx_dsp_config.tx_f_center
		if as_offset:
			return f_use_abs - self.tx_dsp_config.tx_f_tune
		return f_use_abs


	def _get_transmit_baudrate(self):
		if (not self.baudrate_following) or (not self.last_verified_baudrate):
			return self.tx_dsp_config.baudrate
		return self.last_verified_baudrate


	def _compose_samples(self, payload, usrp_reshape, as_c64):
		t00 = time.perf_counter()
		baudrate = self._get_transmit_baudrate()
		f_use_offset = self._get_transmit_frequency(as_offset=True)
		f_offset_nrm = f_use_offset / self.tx_dsp_config.tx_sr0
		pl_char_ints = np.array(bytearray(payload), dtype=np.int64)
		bits = frame_packet(pl=pl_char_ints, synchword_int=DEFAULT_SYNCHWORD, synchword_len=DEFAULT_SYNCHWORD_LEN, use_scrambler=True, use_rs=True, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg, nrz_shift=True)
		bits = np.concatenate( (self.preamble_bits, bits) )
		sps = self.tx_dsp_config.tx_sr0 / baudrate
		n_silence_start = int(self.tx_dsp_config.tx_sr0 * 2.0e-3) # TODO: this should be a setting?
		dt1 = time.perf_counter() - t00
		t00 = time.perf_counter()
		samples, _ = make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_nrm, power=1.0, mod_index=self.tx_dsp_config.tx_mod_index, shaper_BT_prod=self.tx_dsp_config.tx_BT, n_silence_start=n_silence_start, n_silence_end=0)
		#samples = np.exp(2j*np.pi*np.arange(len(samples)) * 0.005 )
		dt2 = time.perf_counter() - t00
		t00 = time.perf_counter()
		if usrp_reshape:
			samples = np.reshape(samples, (1, len(samples)))
		if as_c64:
			samples = np.array(samples, dtype=np.complex64)
		dt3 = time.perf_counter() - t00
		return samples, f_use_offset+self.tx_dsp_config.tx_f_tune, (dt1, dt2, dt3)
	# ========================================================================================================================================================================================
	# == private functions ===================================================================================================================================================================



	# -- loops -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	def _rx_loop(self):
		default_batchlen = self.rx_dsp_config.batch_maxlen // 2
		T_sample = 1.0 / self.rx_dsp_config.rx_sr0
		while self.on:
			unix_minus_mono = time.time() - time.monotonic()
			try:
				ts_s0_mono, samples = self.que_rx_samples_in.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Queue.get() exception in ReceiverLoop run: ", e)
				self.on = False
				break
			with self.rlock:
				c = 0
				while c < len(samples):
					batch = samples[c:c+default_batchlen]
					rx_pls, carrier_sensed = self.rx.process_samples(batch=batch, give_bits=False)
					ts_mono = ts_s0_mono + c * T_sample
					ts_unix = ts_mono + unix_minus_mono
					if carrier_sensed and (ts_mono > self.t_projected_tx_end):
						self.que_rcv_payloads_out.put( ("cs", None), timeout=1.0)
					for rx_pl, rx_f_absolute, power_tuple in rx_pls:
						if rx_pl in self.own_recently_sent:
							#del self.own_recently_sent[rx_pl]
							DBGPRINT("Discarded self reception.")
							continue
						DBGPRINT("RX-PL: {} bytes,   {} MHz,   {} SNR".format(len(rx_pl), round(rx_f_absolute*1e-6, 3), round(snr_dB(pl_power=power_tuple[0], noise_power=power_tuple[1]), 2)))
						self.last_verified_freq = (rx_f_absolute, time.monotonic())
						self.last_verified_baudrate = self.rx_dsp_config.baudrate
						self.que_rcv_payloads_out.put( ("pl", rx_pl), timeout=1.0)
						self.que_signaldata_out.put((ts_unix, rx_f_absolute, power_tuple, self.rx_dsp_config.baudrate, rx_pl), timeout=1.0)
					c += default_batchlen


	def _tx_loop(self):
		while self.on:
			if not self.que_tx_samples_out.empty():
				time.sleep(0.002)
				continue
			try:
				payload = self.que_tx_payloads_in.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Queue.get() exception (tx-thread):", e)
				self.on = False
				break
			with self.rlock:
				assert type(payload) in (bytes, bytearray)
				self._clean_own_sent()
				self.own_recently_sent[payload] = time.monotonic()
				samplearr, f_use_abs, _ = self._compose_samples(payload, usrp_reshape=True, as_c64=True)
			DBGPRINT("TX Start at {} MHz".format( f_use_abs * 1e-6, 3))
			self.que_tx_samples_out.put(samplearr, timeout=4.0)
			self.t_projected_tx_end = time.monotonic() + 5e-3 + samplearr.shape[1] / self.tx_dsp_config.tx_sr0


	def _rx_loop_mpr(self, dsp_config_list, ring_len):
		que_mpr_processes_out = mpr.Queue(256)
		buffer_ring = list()
		arrlen = int(2**21)
		buffer_shm_list = list()
		for _ in range(ring_len):
			shm = shared_memory.SharedMemory(create=True, size=np.zeros(arrlen, dtype=np.complex128).nbytes)
			arr = np.ndarray(shape=arrlen, dtype=np.complex128, buffer=shm.buf)
			arr[:] = 0.0
			buffer_ring.append(arr)
			buffer_shm_list.append(shm)
		flag_ring_shm = shared_memory.SharedMemory(create=True, size=np.zeros((ring_len,3), dtype=np.int64).nbytes )
		ring_flag_arr = np.ndarray(shape=(ring_len, 3), dtype=np.int64, buffer=flag_ring_shm.buf) # (batch_counter, nsamples, ts_s0_mono/unix_ns)
		ring_flag_arr[:] = -1
		processes = dict()
		process_events = list()
		for i in range(len(dsp_config_list)):
			mpr_ev = mpr.Event()
			mpr_ev.clear()
			process_events.append(mpr_ev)
			process_idd = len(processes)
			p = mpr.Process(target=_rx_mpr_process, args=(dsp_config_list[i], process_idd, mpr_ev, [shm.name for shm in buffer_shm_list], flag_ring_shm.name, que_mpr_processes_out), daemon=True) #dsp_config:RXDSPConfig, idd, trig_ev, shm_buffer_ring_shm_names, flag_ring_shm_name, que_out
			processes[process_idd] = p
			p.start()
		batch_index = 0
		ring_head = 0
		while self.on:
			try:
				ts_s0_mono, samples = self.que_rx_samples_in.get(timeout=0.20)
				nsamples = len(samples)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Queue.get() exception in ReceiverLoop run: ", e)
				self.on = False
				break
			with self.rlock:
				buffer_ring[ring_head][:nsamples] = samples
				ring_flag_arr[ring_head] = (batch_index, nsamples, int(ts_s0_mono*1e9))
				for mpr_ev in process_events:
					mpr_ev.set()
				while not que_mpr_processes_out.empty():
					rcode, p_idd, tup = que_mpr_processes_out.get_nowait()
					if rcode == "cs":
						self.que_rcv_payloads_out.put( ("cs", None), timeout=1.0)
					elif rcode == "pl":
						(ts_unix, rx_f_absolute, power_tuple, baudrate, rx_pl) = tup
						if rx_pl in self.own_recently_sent:
							#del self.own_recently_sent[rx_pl]
							DBGPRINT("Discarded self reception.")
							continue
						self.last_verified_baudrate = baudrate
						self.last_verified_freq = (rx_f_absolute, time.monotonic())
						self.que_rcv_payloads_out.put( ("pl", rx_pl), timeout=1.0)
						self.que_signaldata_out.put((ts_unix, rx_f_absolute, power_tuple, baudrate, rx_pl), timeout=1.0)
					else:
						raise AssertionError("Unknown rcode from an rx process:", rcode)
		ring_flag_arr[:] = -2
	# ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# -- loops -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------







def _rx_mpr_process(rx_dsp_config:RXDSPConfig, idd, trig_ev, shm_buffer_ring_shm_names, flag_ring_shm_name, que_out):
	default_batchlen = rx_dsp_config.batch_maxlen // 2
	T_sample = 1.0 / rx_dsp_config.rx_sr0
	rx = Receiver(config=rx_dsp_config)
	buffer_ring = list()
	for (name, shape, dtype) in shm_buffer_ring_shm_names:
		shm = shared_memory.SharedMemory(name=name)
		arr = np.ndarray(shape=shape, dtype=dtype, buffer=shm.buf)
		buffer_ring.append(arr)
	ring_len = len(buffer_ring)
	flag_ring_shm = shared_memory.SharedMemory(name=flag_ring_shm_name[0])
	ring_flag_arr = np.ndarray(shape=(ring_len, 3), dtype=np.int64, buffer=flag_ring_shm.buf) # (batch_counter, nsamples, ts_s0_mono/unix_ns)
	ring_head = 0
	last_batch_index = -1
	while True:
		unix_minus_mono = time.time() - time.monotonic()
		trig = trig_ev.wait(timeout=0.20)
		if not trig:
			if ring_flag_arr[0,0] < -1:
				return
			continue
		trig_ev.clear()
		while True:
			if ring_flag_arr[ring_head,0] == -1:
				break
			if ring_flag_arr[ring_head,0] == (last_batch_index - ring_len + 1):
				break
			if (last_batch_index != -1) and (ring_flag_arr[ring_head,0] != (last_batch_index + 1)):
				raise AssertionError("mpr dsp loop fell out of synch: ", (ring_flag_arr[ring_head,0], last_batch_index))
			last_batch_index = ring_flag_arr[ring_head,0]
			nsamples = int(ring_flag_arr[ring_head,1])
			ts_s0_mono = ring_flag_arr[ring_head,2] * 1.0e-9
			buffer_arr = buffer_ring[ring_head]
			c = 0
			while c < nsamples:
				rx_pls, carrier_sensed = rx.process_samples(batch=buffer_arr[c:min(c+default_batchlen,nsamples)] , give_bits=False)
				ts_mono = ts_s0_mono + c * T_sample
				ts_unix = ts_mono + unix_minus_mono
				if carrier_sensed:   # TODO: filter for ongoing own transmission ... "(t_mono > self.t_projected_tx_end)"
					que_out.put( ("cs",idd,None), timeout=1.0)
				for rx_pl, rx_f_absolute, power_tuple in rx_pls:
					que_out.put( ("pl",idd,(ts_unix, rx_f_absolute, power_tuple, rx_dsp_config.baudrate, rx_pl)), timeout=1.0) #(ts_unix, rx_f_absolute, power_tuple, self.dsp_config.baudrate, rx_pl)
				c += default_batchlen
			ring_head = (ring_head+1) % ring_len
























