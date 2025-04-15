import time
import numpy as np
from .lib_demodulation import demodulation_sequence, create_DSD_statemx, DSD_buffer_roll
from .lib_symsynching import create_classic_JPL_statemx
from .lib_fft_finder import create_cont_center_statemx, fft_continuous_f_center, set_f_center_search_map
from .lib_framing import create_deframer, deframe, RS_MAX_ENCODED_LEN, frame_packet, RS_MAX_PL_LEN
from .lib_tools import DEFAULT_SYNCHWORD, DEFAULT_SYNCHWORD_LEN, radionoise, make_samples2, ints_to_bits, freq_shift_phased, choose_fftlen
from .lib_reedsolomon import get_default_rs
from .lib_div_resampler import staged_resampler_execute_stream, create_staged_resampler
from .lib_tools import get_frequency_search_map




class DSPConfig:
	def __init__(self, rx_sr0, rx_f_tune, rx_f_center, tx_sr0, tx_f_tune, tx_f_center, baudrate, bufferlen, batch_maxlen):
		self.bufferlen			= bufferlen
		self.batch_maxlen		= batch_maxlen
		# radio device -------------------------------------
		self.rx_sr0 			= rx_sr0		# Raw samplerate of the radio. Will be downsampled with a rate of baudrate*sps/sr0
		self.rx_f_tune 			= rx_f_tune		# Tuned frequency of the radio in absolute Hz (for example 350.0e6)
		self.tx_sr0				= tx_sr0
		self.tx_f_tune			= tx_f_tune
		# --------------------------------------------------
		# signal properties --------------------------------
		self.rx_f_center 		= rx_f_center	# The (absolute) frequency of the transmissions in absolute Hz (for example 350.12e6)
		self.tx_f_center 		= tx_f_center
		self.baudrate			= baudrate		# Baudrate of the transmission. Has a definite effect on performance. More so if resampling rate is not adjusted.
		# --------------------------------------------------
		# resampling ---------------------------------------
		self.sps 				= 12 			# ! sps (samples-per-symbol) for the signal processing pipeline. Determines resampling rate. Has a _minor_ effect on performance. (See tests_resamples.py)
		self.d_halflen 			= 28			# ! Integer resampling lowpass filter halflen. Larger number increases both accuracy and computation cost. Has a minor effect on performance.
		self.f_halflen 			= 12			# ! Fractional resampling lowpass filter halflen. Larger number increases both accuracy and computation cost. Has a *major* effect on performance.
		self.n_banks 			= 64			# - Number of resampling banks. Almost no effect on performance, and 64 seems good for all purposes.
		self.rs_f_cutoff_coeff 	= 0.499			# - Lowpass associated with the resampling. In interval (0:0.5). 0.499 still enables some aliasing at edges.
		# --------------------------------------------------
		# fft detection ------------------------------------
		self.fftlen_mpr 		= 50			# ! Length of the fft window in multiples of sps in center frequency detector. Larger number increases frequency resolution, but also induces decoding delay.
		self.mod_index 			= 0.5			# S Modulation index. A core FM-modulation parameter. Determines the frequency deviation from center.
		self.BT_rx_match 		= 0.425			# S Bandwidth-Time product of an optional gaussian filter on modulating squarewave. set to -1 for no gaussian filtering. TODO: best match for 0.5 at UHF-firmware is 0.425 here
		self.centering_delay_mpr= 2.15 			# ! Center frequency estimate is collected for (centering_delay_mpr*fftlen) samples ahead of demodulation. TODO should be in symbols?
		self.c_center_decay		= 0.94 			# ! Exponential decay factor of the center frequency correlation sum.
		self.search_halfband	= 20.0e3 		# - Determines the frequency band above and below the center frequency where the demodulator looks for signals. (For 437MHz at orbital speeds, maximum doppler ~11kHz)
		# --------------------------------------------------
		# JPL synchronizer ---------------------------------
		self.JPL_n_decay 		= 55 			# ~ How quickly JPL-synchronizer's accumulator exponentially decays. The values are updated as: acc = (acc + measurement) * (1 - 1/JPL_n_decay)
		# --------------------------------------------------
		# demodulation -------------------------------------
		self.lp_ntaps			= 161			# - number of taps in the low-pass filter in demodulation
		self.lp_cutoff_coeff	= 0.600			# - cutoff frequency of the low-pass filter, as multiples of baudrate
		self.synch_delay_mpr	= 30 			# ~ demodulator decides symbols synch_delay_mpr symboltimes behind the synchronizer. This allows a synch to be found before symbols are decoded.
		# --------------------------------------------------
		# framing ------------------------------------------
		self.use_scrambler 		= True
		self.use_rs 			= True
		self.synch_threshold 	= 3
		self.data_maxlen 		= RS_MAX_ENCODED_LEN
		# --------------------------------------------------
		# --------------------------------------------------
		self.tx_BT				= 0.5
		self.tx_mod_index		= 0.5
		# --------------------------------------------------

	def check_validity(self):
		assert 10000 < self.bufferlen < 100e6
		assert type(self.bufferlen) == int
		assert 100 < self.batch_maxlen < (0.05*self.bufferlen)
		assert type(self.batch_maxlen) == int

		for (sr0,f_tune,f_center) in [(self.rx_sr0, self.rx_f_tune, self.rx_f_center), (self.tx_sr0, self.tx_f_tune, self.tx_f_center)]:
			assert 1e3 < sr0 < 32e6
			assert f_tune >= 1.0
			assert f_center >= 1.0
			assert 0 < self.baudrate < (sr0/2)
		assert (abs(self.rx_f_tune - self.rx_f_center) + self.search_halfband + self.baudrate * 0.6) < (0.5 * self.rx_sr0), "Radio tuned to this frequency with this samplerate cannot see the entire band."
		assert ((self.search_halfband + self.baudrate*0.6) / (self.baudrate * self.sps)) < 0.5, "Resampling down to this sps at this baudrate cannot see the entire search band."

		assert 2 < self.sps <= 100
		assert type(self.sps) == int
		assert 4 < self.d_halflen < 42
		assert (self.d_halflen*2) > int(self.rx_sr0 / (self.baudrate * self.sps)),  (self.d_halflen*2, int(self.rx_sr0 / (self.baudrate * self.sps)))
		assert type(self.d_halflen) == int
		assert 4 < self.f_halflen < 42
		assert type(self.f_halflen) == int
		assert 24 < self.n_banks < 240
		assert type(self.n_banks) == int
		assert 0 < self.rs_f_cutoff_coeff < 0.5
		assert type(self.fftlen_mpr) == int
		assert 3 < self.fftlen_mpr < 150
		assert 0.5 <= self.mod_index < 10.0
		assert (self.BT_rx_match >= 0.4) or (self.BT_rx_match == -1)  # Canonically BT should never be under 0.5. However, using 0.425 when generating samples produces best match to recordings from UHF with CC1125 chip.
		assert 1 <= self.centering_delay_mpr < 20
		assert 0.6 < self.c_center_decay < 1.0
		assert 1.0 <= self.JPL_n_decay < 100.0
		assert 30.0 <= self.lp_ntaps < 300.0
		assert type(self.lp_ntaps) == int
		assert (self.lp_ntaps % 2) == 1
		assert 0 < self.lp_cutoff_coeff < (0.5*self.sps)
		assert 0 <= self.synch_delay_mpr <= 64.0
		assert type(self.use_scrambler) == bool
		assert type(self.use_rs) == bool
		assert type(self.synch_threshold) == int
		assert 0 <= self.synch_threshold < 5
		assert type(self.data_maxlen) == int
		assert self.data_maxlen > 10
		if self.use_rs:
			assert self.data_maxlen == RS_MAX_ENCODED_LEN
		assert (self.tx_BT >= 0.4) or (self.tx_BT == -1)
		assert 0.5 <= self.tx_mod_index < 10.0

	def get_r_rate(self):
		r_rate = self.sps * self.baudrate / self.rx_sr0
		return r_rate

	def get_f_center_search_map(self, fftlen, is_precentered:bool):
		if is_precentered:
			f_center_min_nrm = -self.search_halfband / (self.baudrate * self.sps)
			f_center_max_nrm = self.search_halfband / (self.baudrate * self.sps)
		else:
			f_center_min_nrm = (self.rx_f_center - self.search_halfband - self.rx_f_tune) / (self.baudrate * self.sps)
			f_center_max_nrm = (self.rx_f_center + self.search_halfband - self.rx_f_tune) / (self.baudrate * self.sps)
		f_center_search_map = get_frequency_search_map(fftlen=fftlen, f_min_nrm=f_center_min_nrm, f_max_nrm=f_center_max_nrm, assert_in_window=True)
		return f_center_search_map








class Receiver:
	def __init__(self, config:DSPConfig):
		config.check_validity()
		self.config 			= config
		self.bufferlen 			= int(config.bufferlen)
		self.rs_array 			= np.zeros(self.bufferlen, dtype=np.complex128)
		self.center_f_array 	= np.zeros(self.bufferlen, dtype=np.float64)
		self.dmd_array 			= np.zeros(self.bufferlen, dtype=np.float64)
		self.synch_array 		= np.zeros((self.bufferlen, 3), dtype=np.int64)
		self.bit_array 			= np.zeros(self.bufferlen // 10, dtype=np.int64)
		self.bit_f_array 		= np.zeros(self.bufferlen // 10, dtype=np.float64)
		self.fft_instr_array 	= np.zeros((self.bufferlen, 3), dtype=np.float64)
		self.bufferhalf			= int(self.bufferlen / 2)
		self.buffer_roll_limit 	= int(self.bufferlen*3/4)
		self.rs_head 			= 0
		self.center_f_head 		= 0
		self.demodulation_head 	= 0
		self.dmdsynch_head 		= 0
		self.bit_head 			= 0
		self.rsmpl_mx1 			= np.zeros((2,2), dtype=np.float64)
		self.rsmpl_mx2 			= np.zeros((2,2), dtype=np.float64)
		self.FFTstatemx 		= np.zeros((2,2), dtype=np.float64)
		self.JPLstatemx 		= np.zeros((2,2), dtype=np.float64)
		self.DSDstatemx 		= np.zeros((2,2), dtype=np.float64)
		self.deframermx 		= np.zeros((2,2), dtype=np.float64)
		rs_mx, rs_cfg 			= get_default_rs()
		self.rs_mx 				= rs_mx
		self.rs_cfg 			= rs_cfg
		self.centering_fdelta_nrm = 0.0
		self.centering_phase 	= 0.0
		self.dt_array			= np.zeros(6, dtype=np.float64)
		self._setup()
		# This series of baudrate switches pre-generates correlation masks to memory.
		#_br = self.config.baudrate
		#_sps = self.config.sps
		#self.switch_baudrate(baudrate=9600, sps=_sps)
		#self.switch_baudrate(baudrate=9600*2, sps=_sps)
		#self.switch_baudrate(baudrate=9600*2*2, sps=_sps)
		#self.switch_baudrate(baudrate=_br, sps=_sps)

	def _setup(self):
		self.config.check_validity()
		config = self.config
		fftlen, _ = choose_fftlen(config.fftlen_mpr*config.sps, window_halfwid=int(0.06*config.fftlen_mpr*config.sps))
		f_cutoff = config.get_r_rate() * config.rs_f_cutoff_coeff
		f_center_search_map = config.get_f_center_search_map(fftlen=fftlen, is_precentered=True)
		self.centering_fdelta_nrm = -(config.rx_f_center - config.rx_f_tune) / config.rx_sr0
		rsmpl_mx1, rsmpl_mx2 = create_staged_resampler(halflen_div=config.d_halflen, halflen_f=config.f_halflen, r_rate=config.get_r_rate(), n_banks=config.n_banks, f_cutoff=f_cutoff, allow_aliasing=False)
		self.rsmpl_mx1 = rsmpl_mx1
		self.rsmpl_mx2 = rsmpl_mx2
		self.FFTstatemx = create_cont_center_statemx(fftlen=fftlen, sps=config.sps, f_center_search_map=f_center_search_map, mod_index=config.mod_index,
														BT_rx_match=config.BT_rx_match, centering_delay_mpr=config.centering_delay_mpr, c_center_decay=config.c_center_decay)
		self.JPLstatemx = create_classic_JPL_statemx(N_eps=config.sps, n_decay=config.JPL_n_decay)
		self.DSDstatemx = create_DSD_statemx(lp_ntaps=config.lp_ntaps, lp_cutoff_coeff=config.lp_cutoff_coeff, synch_delay_mpr_f=config.synch_delay_mpr, sps_f=config.sps)
		self.deframermx = create_deframer(use_scrambler=config.use_scrambler, use_rs=config.use_rs, data_maxlen=config.data_maxlen,
										  synchword=DEFAULT_SYNCHWORD, synchword_len=DEFAULT_SYNCHWORD_LEN, synch_threshold=config.synch_threshold)
		a = int(config.batch_maxlen * config.get_r_rate() * 2)
		b = fftlen * 2
		self.buffer_roll_limit 	= self.bufferlen - (a + b + 4)
		assert self.buffer_roll_limit > (self.bufferlen * 0.9), self.buffer_roll_limit/self.bufferlen
		self.rs_head 			= 0
		self.center_f_head 		= 0
		self.demodulation_head 	= 0
		self.dmdsynch_head 		= 0
		self.bit_head 			= 0
		self.centering_phase 	= 0.0
		self.dt_array *= 0.0


	def get_fftlen(self):
		fftlen1  = int(self.FFTstatemx[0,0])
		fftlen2  = int(self.FFTstatemx.shape[1])
		assert fftlen1 == fftlen2
		assert fftlen1 >= 20
		return fftlen1

	def set_search_map(self, f_center_search_map):
		set_f_center_search_map(statemx=self.FFTstatemx, search_map=f_center_search_map)


	def switch_baudrate(self, baudrate, sps):
		assert baudrate > 0
		assert type(sps) == int
		assert sps > 1
		self.config.baudrate = baudrate
		self.config.sps = sps
		self.config.check_validity()
		self._setup()


	def process_samples(self, batch, give_bits=False):
		ret = list()
		bits = np.zeros(0, dtype=np.int64)

		t0 = time.perf_counter()
		batch2, self.centering_phase = freq_shift_phased(batch, sr=1.0, fdelta=self.centering_fdelta_nrm, phase0=self.centering_phase)
		self.dt_array[0] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		rs_head_new = staged_resampler_execute_stream(in_arr=batch2, ii0=0, nsamples=len(batch2), out_arr=self.rs_array, io0=self.rs_head, mx1=self.rsmpl_mx1, mx2=self.rsmpl_mx2)
		self.dt_array[1] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		center_f_head_new, demodulation_head_new = fft_continuous_f_center(sample_arr=self.rs_array, isample0=self.rs_head, nsamples=rs_head_new - self.rs_head,
																				center_f_arr=self.center_f_array, center_f_head0=self.center_f_head,
																				statemx=self.FFTstatemx, instr_arr=self.fft_instr_array)
		##self.center_f_array[self.demodulation_head:demodulation_head_new] = 0.152
		self.dt_array[2] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		dmdsynch_head_new, bit_head_new = demodulation_sequence(rs_arr=self.rs_array, centerf_arr=self.center_f_array, i_rs0=self.demodulation_head,
																nsamples=demodulation_head_new - self.demodulation_head, dmd_arr=self.dmd_array, synch_arr=self.synch_array,
																dmdsynch_head0=self.dmdsynch_head, JPLstatemx=self.JPLstatemx, demodmx=self.DSDstatemx,
																bitarr=self.bit_array, bitfarr=self.bit_f_array, bit_head0=0)
		self.dt_array[3] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		if bit_head_new > 0:
			bits = self.bit_array[:bit_head_new]
			bit_freqs = self.bit_f_array[:bit_head_new]
			if not give_bits:
				bits = np.clip(bits, 0, 1)
				payloads, payload_delimits, payload_frequencies = deframe(bits=bits, bit_frequencies=bit_freqs, deframer_mx=self.deframermx, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg)
				if len(payload_delimits) > 0:
					ret = self._split_payloads(payloads=payloads, delimits=payload_delimits, nrm_offset_frequencies=payload_frequencies)
		self.dt_array[4] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		self.rs_head = rs_head_new
		self.center_f_head = center_f_head_new
		self.demodulation_head = demodulation_head_new
		self.dmdsynch_head = dmdsynch_head_new
		assert self.rs_head == self.center_f_head
		if self.rs_head >= self.buffer_roll_limit:
			self._buffer_roll_1()
		if self.dmdsynch_head >= self.buffer_roll_limit:
			self._buffer_roll_2()
		self.dt_array[5] += (time.perf_counter() - t0)

		if give_bits:
			return bits
		return ret


	def _split_payloads(self, payloads, delimits, nrm_offset_frequencies):
		pl_list = list()
		for i_pl, (i0,i1) in enumerate(delimits):
			#f_offset_nrm = nrm_offset_frequencies[i_pl] + self.centering_fdelta_nrm
			f_absolute = (nrm_offset_frequencies[i_pl] * self.config.sps * self.config.baudrate) + self.config.rx_f_center
			pl_list.append((bytes(payloads[i0:i1]), f_absolute))
			assert len(pl_list[-1][0]) == (i1-i0)
		return pl_list


	def _buffer_roll_1(self):
		self.rs_array[0:self.rs_head-self.bufferhalf] 				= self.rs_array[self.bufferhalf:self.rs_head]
		self.center_f_array[0:self.center_f_head-self.bufferhalf] 	= self.center_f_array[self.bufferhalf:self.center_f_head]
		self.fft_instr_array[0:self.center_f_head-self.bufferhalf] 	= self.fft_instr_array[self.bufferhalf:self.center_f_head]
		self.rs_head 			= self.rs_head - self.bufferhalf
		self.center_f_head 		= self.center_f_head - self.bufferhalf
		self.demodulation_head 	= self.demodulation_head - self.bufferhalf


	def _buffer_roll_2(self):
		self.dmd_array[0:self.dmdsynch_head-self.bufferhalf] 		= self.dmd_array[self.bufferhalf:self.dmdsynch_head]
		self.synch_array[0:self.dmdsynch_head-self.bufferhalf] 		= self.synch_array[self.bufferhalf:self.dmdsynch_head]
		self.dmdsynch_head = self.dmdsynch_head - self.bufferhalf
		DSD_buffer_roll(demodmx=self.DSDstatemx, buffers_receded_by=self.bufferhalf)







# PRECOMPILE RECEIVER ====================================================================================================
def get_a_precompiling_sampleset(dsp_config:DSPConfig, do_print=False):
	sr0 = dsp_config.rx_sr0
	rel_offset_raw = (dsp_config.rx_f_center-dsp_config.rx_f_tune) / sr0  #0.1 * (sps*baudrate/sr0)
	rs_mx, rs_cfg = get_default_rs()
	pl = np.random.randint(0,255, RS_MAX_PL_LEN-2)
	preamble_bits = ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
	frame_bits = frame_packet(pl=pl, synchword_int=DEFAULT_SYNCHWORD, synchword_len=DEFAULT_SYNCHWORD_LEN, use_scrambler=True, use_rs=True, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=True)
	bitstring = np.concatenate( (preamble_bits, frame_bits) )
	transmission, _ = make_samples2(sps_f=sr0/dsp_config.baudrate, bitstring=bitstring, f_offset=rel_offset_raw, power=1.0, mod_index=dsp_config.mod_index, shaper_mode=1, shaper_BT_prod=dsp_config.BT_rx_match, n_silence_start=0, n_silence_end=0)

	n_fft_calibration = int( (sr0/(dsp_config.baudrate*dsp_config.sps)) * 2*dsp_config.fftlen_mpr*dsp_config.sps )
	nsamples = int(n_fft_calibration + len(transmission) + 1.0*sr0)
	i0 = int(n_fft_calibration)
	if do_print:
		print("\tEquivalent times per sample segment:")
		print("\t\t{} s for fft-calibration".format( round( n_fft_calibration/sr0 , 3 ) ))
		print("\t\t{} s for transmission".format( round( len(transmission)/sr0, 3)))
		print("\t\t{} s for margins".format( round(2.0, 3) ))
	noiseless = np.zeros(nsamples, dtype=np.complex128)
	noiseless[i0:i0+len(transmission)] += transmission
	noisePpHz = 0.02/dsp_config.baudrate
	noise = radionoise(n=nsamples, sr=sr0, W_per_Hz=noisePpHz)
	return noiseless, noise, noisePpHz


def precompile_receiver(dsp_config:DSPConfig, do_print=False):
	if do_print:
		print("[Precompiling]")
	t0 = time.perf_counter()
	if do_print:
		print("\t[Generating sampleset]")
	noiseless, noise, noisePpHz = get_a_precompiling_sampleset(dsp_config, do_print)
	samples = np.array(noiseless+noise, dtype=np.complex128)
	t1 = time.perf_counter()
	nsamples = len(samples)
	rx1 = Receiver(config=dsp_config)
	rx2 = Receiver(config=dsp_config)
	c = 0
	ret_pls1 = list()
	ret_pls2 = list()
	if do_print:
		print("\t[Processing]")
	while c < nsamples:
		c2 = min(c + dsp_config.batch_maxlen // 2, nsamples)
		batch = samples[c:c2]
		ret1 = rx1.process_samples(batch=batch, give_bits=False)
		ret2 = rx2.process_samples(batch=np.complex64(batch), give_bits=False)
		ret_pls1.extend(ret1)
		ret_pls2.extend(ret2)
		c = c2
	t2 = time.perf_counter()
	#if not ((len(ret_pls1) == 1) and (len(ret_pls2) == 1)):
	#	import pickle
	#	dd = {
	#		"noiseless":noiseless,
	#		"config":rx_config.__dict__,
	#		"noisePpHz":noisePpHz,
	#		"only_noise":noise
	#	}
	#	letters = "".join([chr(x) for x in np.random.randint(ord("A"), ord("Z")+1, 3)])
	#	f = open("precompile_fail_samples_and_config_{}.pkl".format(letters), "wb")
	#	f.write(pickle.dumps(dd))
	#	f.close()
	#	print("Repro data written for ",letters)
	assert len(ret_pls1) == 1, len(ret_pls1)
	assert len(ret_pls2) == 1, len(ret_pls2)
	if do_print:
		print("\t[Precompiled in {} s.  ({} s for samples)]".format( round(t2-t0, 3), round(t1-t0, 3)  ))
# PRECOMPILE RECEIVER ====================================================================================================
# @:374







