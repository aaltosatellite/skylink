import queue
import numpy as np
from .lib_receiver import Receiver, DSPConfig, precompile_receiver
from .lib_framing import frame_packet
from .lib_tools import doppler_correction, ints_to_bits, DEFAULT_SYNCHWORD_LEN, DEFAULT_SYNCHWORD, make_samples2
from .lib_reedsolomon import get_default_rs
from datetime import datetime as dtime
import threading
from queue import Queue, Empty
import time



DEBUG_PRINT_ON = True
def DBGPRINT(*args, **kwargs):
	ts = "[{}]".format( dtime.now().isoformat()[-15:] )
	ts += " "*(17-len(ts)) + "[DSPLoop]" + "   "
	first, args = args[0], args[1:]
	if DEBUG_PRINT_ON:
		print(ts+str(first), *args, **kwargs)



class DSPLoop:
	def __init__(self, dsp_config:DSPConfig, que_rx_samples_in:Queue, que_rx_payloads_out:queue.Queue, que_tx_payloads_in:Queue, que_tx_samples_out:Queue):
		DBGPRINT("Precompile DSP")
		precompile_receiver(dsp_config, do_print=False)
		self.dsp_config 			= dsp_config
		self.frequency_following 	= True
		self.use_doppler_correction = True
		self.preamble_bits 			= ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
		rs_mx, rs_cfg 				= get_default_rs()
		self.rs_mx 					= rs_mx
		self.rs_cfg 				= rs_cfg
		self.rlock 					= threading.RLock()
		self.que_rx_samples_in		= que_rx_samples_in
		self.que_rcv_payloads_out	= que_rx_payloads_out
		self.que_tx_payloads_in		= que_tx_payloads_in
		self.que_tx_samples_out		= que_tx_samples_out
		self.rx 				= Receiver(config=dsp_config)
		self.rx_process_thread 	= threading.Thread(target=None, args=tuple())
		self.tx_process_thread 	= threading.Thread(target=None, args=tuple())
		self.last_verified_freq = (0, 0.0)  # (absolute_frequency, monotonic_timestamp)
		self.own_recently_sent 	= dict()
		self.on 				= True


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
			self.dsp_config.baudrate = baudrate
			self.rx.switch_baudrate(baudrate, sps=self.rx.config.sps)


	def set_doppler_correction(self, toggle:bool):
		with self.rlock:
			self.use_doppler_correction = bool(toggle)


	def get_state(self):
		with self.rlock:
			d_state = dict()
			d_state["baudrate"] = self.dsp_config.baudrate
			d_state["frequency-following-on"] = self.frequency_following
			d_state["doppler-correction-on"] = self.use_doppler_correction
			d_state["f-sdr-tune"] = self.dsp_config.rx_f_tune
			d_state["f-center"] = self.dsp_config.rx_f_center
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
				f_use_abs, _ = doppler_correction(f_received=f_recv_abs, f_original=self.dsp_config.rx_f_center, f_at_target=self.dsp_config.rx_f_center)
			else:
				f_use_abs = f_recv_abs
		else:
			f_use_abs = self.dsp_config.rx_f_center
		if as_offset:
			return f_use_abs - self.dsp_config.rx_f_tune
		return f_use_abs


	def _compose_samples(self, payload, usrp_reshape, as_c64):
		t00 = time.perf_counter()
		f_use_offset = self._get_transmit_frequency(as_offset=True)
		f_offset_nrm = f_use_offset / self.dsp_config.tx_sr0
		pl_char_ints = np.array(bytearray(payload), dtype=np.int64)
		bits = frame_packet(pl=pl_char_ints, synchword_int=DEFAULT_SYNCHWORD, synchword_len=DEFAULT_SYNCHWORD_LEN, use_scrambler=True, use_rs=True, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg, nrz_shift=True)
		bits = np.concatenate( (self.preamble_bits, bits) )
		sps = self.dsp_config.tx_sr0 / self.dsp_config.baudrate
		n_silence_start = int(self.dsp_config.tx_sr0 * 2.0e-3) # TODO: this should be a setting?
		dt1 = time.perf_counter() - t00
		t00 = time.perf_counter()
		samples, _ = make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_nrm, power=1.0, mod_index=self.dsp_config.tx_mod_index, shaper_mode=1,
							   shaper_BT_prod=self.dsp_config.tx_BT, n_silence_start=n_silence_start, n_silence_end=0)
		#samples = np.exp(2j*np.pi*np.arange(len(samples)) * 0.005 )
		dt2 = time.perf_counter() - t00
		t00 = time.perf_counter()
		if usrp_reshape:
			samples = np.reshape(samples, (1, len(samples)))
		if as_c64:
			samples = np.array(samples, dtype=np.complex64)
		dt3 = time.perf_counter() - t00
		return samples, f_use_offset+self.dsp_config.tx_f_tune, (dt1, dt2, dt3)


	def _rx_loop(self):
		default_batchlen = self.dsp_config.batch_maxlen // 2
		while self.on:
			try:
				samples = self.que_rx_samples_in.get(timeout=0.20)
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
					rx_pls = self.rx.process_samples(batch=batch, give_bits=False)
					for rx_pl, rx_f_absolute in rx_pls:
						if rx_pl in self.own_recently_sent:
							del self.own_recently_sent[rx_pl]
							DBGPRINT("Discarded self reception.")
							continue
						self.last_verified_freq = (rx_f_absolute, time.monotonic())
						self.que_rcv_payloads_out.put( (rx_pl, rx_f_absolute) )
					c += default_batchlen


	def _tx_loop(self):
		while self.on:
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
			DBGPRINT("tx start at {} MHz".format( f_use_abs * 1e-6, 4 ))
			self.que_tx_samples_out.put(samplearr)









