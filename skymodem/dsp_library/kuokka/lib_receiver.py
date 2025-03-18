import time
import numpy as np
from .lib_demodulation import demodulation_sequence, create_DSD_statemx, DSD_buffer_roll
from .lib_symsynching import create_classic_JPL_statemx
from .lib_fft_detector import fft_detect_and_freq_determ, create_fft_centering_statemx, set_f_center_search_map, get_masklen
from .lib_framing import create_deframer, deframe, RS_MAX_ENCODED_LEN, frame_packet, RS_MAX_PL_LEN
from .lib_tools import DEFAULT_SYNCHWORD, DEFAULT_SYNCHWORD_LEN, radionoise, make_samples, get_frequency_search_map, get_doppler_low_high
from .lib_reedsolomon import get_default_rs
from .lib_resampler import resampler_execute_stream, create_resampler


class ReceiverConfig:
	def __init__(self, sr0, baudrate, bufferlen, batch_maxlen, f_tune, f_center):
		assert sr0 > (baudrate * 2)
		self.f_tune 			= f_tune		# Tuned frequency of the radio in absolute Hz (for example 350.0e6)
		self.f_center 			= f_center		# The (absolute) frequency of the transmissions in absolute Hz (for example 350.12e6)
		self.sr0 				= sr0			# Raw samplerate of the radio. Will be downsampled with a rate of baudrate*sps/sr0
		self.baudrate			= baudrate		# Baudrate of the transmission
		self.bufferlen			= bufferlen
		self.batch_maxlen		= batch_maxlen
		# resampling ---------------------------------------
		self.sps 				= 21			# ? sps (samples-per-symbol) for the signal processing pipeline. Determines resampling rate. Effect on performance seems suspiciously small...
		self.m_halflen 			= 21			# ? Determines resampling accuracy. Should be an odd integer larger than 9. Larger number increases both accuracy and computation cost.
		self.n_banks 			= 64			# ~ Number of resampling banks. No huge effect on performance, and 64 seems good for all purposes.
		self.rs_f_cutoff_coeff 	= 0.499			# ~ Lowpass associated with the resampling. In interval (0:0.5). 0.499 still enables some aliasing at edges.
		# --------------------------------------------------
		# fft detection ------------------------------------
		self.fftlen 			= 1024			# ? Length of the fft window in center frequency detector. Larger number increases frequency resolution, but also induces decoding delay.
		self.mod_index 			= 0.5			# P Modulation index. A core FM-modulation parameter. Determines the frequency deviation from center.
		self.BT 				= 0.5			# P Bandwidth-Time product of an optional gaussian filter on modulating squarewave. set to -1 for no gaussian filtering.
		self.c_stat_update 		= 1/700.0		# ? Running average and variance of the fft-mask correlation are updated by this coeff, as per:  avg = avg + (measurement - avg) * c_stat_update
		self.n_delay			= 1024*5		# ! Center frequency estimate is collected for this many samples ahead of demodulation. TODO should be in symbols?
		self.fft_trigger_on_level 	= 7.0		# ! Signal detection threshold in fft-mask correlation. Units in 'standard deviations above average' (non-software-optimizable?) (was 5.5)
		self.fft_trigger_off_level 	= 2.0		# ! Signal off threshold in fft-mask correlation. Units in 'standard deviations above average' (non-software-optimizable?)
		self.mask_mode 			= 1				# ! Type of correlation mask used in detection. 0: a vector of ones. 1: an empirically averaged mask. 1 should be more performant.
		self.start_margin_mpr 	= 3.0			# ! A signal detection is extended back by (start_margin_mpr*fftlen) samples to time before detection. TODO should be in symbols?
		self.end_margin_mpr 	= 1.5			# ! A signal detection is extended forward by (start_margin_mpr*fftlen) samples to time after detection. TODO should be in symbols?
		self.doppler_velocity	= 7500.0*2.0	# ~ Determines the frequency band above and below the center frequency where the demodulator looks for signals.
		# --------------------------------------------------
		# JPL synchronizer ---------------------------------
		self.JPL_n_decay 		= 32.0			# ! How quickly JPL-synchronizer's accumulator exponentially decays. The values are updated as: acc = (acc + measurement) * (1 - 1/JPL_n_decay)
		# --------------------------------------------------
		# demodulation -------------------------------------
		self.lp_ntaps			= 161			# ? number of taps in the low-pass filter in demodulation
		self.lp_cutoff_coeff	= 0.630			# ! cutoff frequency of the low-pass filter, as multiples of baudrate
		self.synch_delay_mpr	= 16.0			# ! demodulator decides symbols synch_delay_mpr symboltimes behind the synchronizer. This allows a synch to be found before symbols are decoded.
		# --------------------------------------------------
		# framing ------------------------------------------
		self.use_scrambler 		= True
		self.use_rs 			= True
		self.synch_threshold 	= 3
		self.data_maxlen 		= RS_MAX_ENCODED_LEN
		# --------------------------------------------------

	def check_validity(self):
		assert self.f_tune >= 1.0
		assert self.f_center >= 1.0
		assert 1e3 < self.sr0 < 32e6
		assert abs(self.f_tune - self.f_center) < (0.5 * self.sps * self.baudrate)
		fft_df = (self.sps * self.baudrate) / self.fftlen
		halfmask_bw = (1 + get_masklen(fftlen=self.fftlen, sps=self.sps, mask_mode=self.mask_mode)//2) * fft_df
		f_center_min, f_center_max = get_doppler_low_high(f_center=self.f_center, v_relative=self.doppler_velocity, multiplier=1.0)
		f_center_min_nrm = (f_center_min - halfmask_bw - self.f_tune) / (self.baudrate * self.sps)
		f_center_max_nrm = (f_center_max + halfmask_bw - self.f_tune) / (self.baudrate * self.sps)
		assert abs(f_center_min_nrm) < 0.5
		assert abs(f_center_max_nrm) < 0.5

		assert 0 < self.baudrate < (self.sr0/2)
		assert 10000 < self.bufferlen < 100e6
		assert type(self.bufferlen) == int
		assert 100 < self.batch_maxlen < (0.05*self.bufferlen)
		assert type(self.batch_maxlen) == int
		assert 2 < self.sps <= 100
		assert type(self.sps) == int
		assert 5 < self.m_halflen < 42
		assert type(self.m_halflen) == int
		assert (self.m_halflen%2) == 1
		assert 24 < self.n_banks < 240
		assert type(self.n_banks) == int
		assert 0 < self.rs_f_cutoff_coeff < 0.5
		assert self.fftlen in (256, 512, 1024, 2048)
		assert type(self.fftlen) == int
		assert 0.5 <= self.mod_index < 10.0
		assert type(self.BT) in (float, int)
		assert (self.BT >= 0.5) or (self.BT == -1)
		assert 0 < self.c_stat_update <= 1.0
		assert type(self.n_delay) == int
		assert self.fftlen <= self.n_delay < self.fftlen*20
		assert self.n_delay > (self.start_margin_mpr*self.fftlen)	###
		assert -1.0 <= self.fft_trigger_on_level <= 32.0
		assert -1.0 <= self.fft_trigger_off_level <= 9.0
		assert self.fft_trigger_off_level <= self.fft_trigger_on_level
		assert self.mask_mode in (0,1)
		assert 0 <= self.start_margin_mpr < 10.0
		assert 0 <= self.end_margin_mpr < 10.0
		assert 1.0 <= self.JPL_n_decay < 100.0
		assert 30.0 <= self.lp_ntaps < 300.0
		assert type(self.lp_ntaps) == int
		assert (self.lp_ntaps % 2) == 1
		assert 0 < self.lp_cutoff_coeff < (0.5*self.sps)
		assert 0 <= self.synch_delay_mpr <= 64.0
		assert (self.synch_delay_mpr*self.sps) < (self.end_margin_mpr*self.fftlen)  ###
		assert type(self.use_scrambler) == bool
		assert type(self.use_rs) == bool
		assert type(self.synch_threshold) == int
		assert 0 <= self.synch_threshold < 5
		assert type(self.data_maxlen) == int
		assert self.data_maxlen > 10
		if self.use_rs:
			assert self.data_maxlen == RS_MAX_ENCODED_LEN

	def get_r_rate(self):
		r_rate = self.sps * self.baudrate / self.sr0
		return r_rate

	def get_f_center_search_map(self):
		f_center_min, f_center_max = get_doppler_low_high(f_center=self.f_center, v_relative=self.doppler_velocity, multiplier=1.0)
		f_center_min_nrm = (f_center_min - self.f_tune) / (self.baudrate * self.sps)
		f_center_max_nrm = (f_center_max - self.f_tune) / (self.baudrate * self.sps)
		f_center_search_map = get_frequency_search_map(fftlen=self.fftlen, f_min_nrm=f_center_min_nrm, f_max_nrm=f_center_max_nrm, assert_in_window=True)
		return f_center_search_map








class Receiver:
	def __init__(self, config:ReceiverConfig):
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
		self.resampler_statemx 	= np.zeros((2,2), dtype=np.float64)
		self.FFTstatemx 		= np.zeros((2,2), dtype=np.float64)
		self.JPLstatemx 		= np.zeros((2,2), dtype=np.float64)
		self.DSDstatemx 		= np.zeros((2,2), dtype=np.float64)
		self.deframermx 		= np.zeros((2,2), dtype=np.float64)
		rs_mx, rs_cfg 			= get_default_rs()
		self.rs_mx 				= rs_mx
		self.rs_cfg 			= rs_cfg
		self.dt_array			= np.zeros(5, dtype=np.float64)
		self._setup()
		# This series of baudrate switches pre-generates correlation masks to memory.
		_br = self.config.baudrate
		_sps = self.config.sps
		self.switch_baudrate(baudrate=9600, sps=_sps)
		self.switch_baudrate(baudrate=9600*2, sps=_sps)
		self.switch_baudrate(baudrate=9600*2*2, sps=_sps)
		self.switch_baudrate(baudrate=_br, sps=_sps)

	def _setup(self):
		config = self.config
		f_cutoff = config.get_r_rate() * config.rs_f_cutoff_coeff
		f_center_search_map = config.get_f_center_search_map()
		self.resampler_statemx = create_resampler(m_halflen=config.m_halflen, n_banks=config.n_banks, r_rate=config.get_r_rate(), f_cutoff=f_cutoff, allow_aliasing=False)
		self.FFTstatemx = create_fft_centering_statemx(fftlen=config.fftlen, sps=config.sps, f_center_search_map=f_center_search_map, mod_index=config.mod_index,
													   BT=config.BT, c_stat_update=config.c_stat_update, n_delay=config.n_delay, fft_trigger_on_level=config.fft_trigger_on_level,
													   fft_trigger_off_level=config.fft_trigger_off_level, avg0=0.0, var0=1.0, mask_mode=config.mask_mode,
													   start_margin_mpr=config.start_margin_mpr, end_margin_mpr=config.end_margin_mpr)
		self.JPLstatemx = create_classic_JPL_statemx(N_eps=config.sps, n_decay=config.JPL_n_decay)
		self.DSDstatemx = create_DSD_statemx(lp_ntaps=config.lp_ntaps, lp_cutoff_coeff=config.lp_cutoff_coeff, synch_delay_mpr_f=config.synch_delay_mpr, sps_f=config.sps)
		self.deframermx = create_deframer(use_scrambler=config.use_scrambler, use_rs=config.use_rs, data_maxlen=config.data_maxlen,
										  synchword=DEFAULT_SYNCHWORD, synchword_len=DEFAULT_SYNCHWORD_LEN, synch_threshold=config.synch_threshold)
		a = int(config.batch_maxlen * config.get_r_rate() * 2)
		b = int((config.start_margin_mpr + config.end_margin_mpr + 1) * config.fftlen)
		self.buffer_roll_limit 	= self.bufferlen - (a + b + 4)
		assert self.buffer_roll_limit > (self.bufferlen * 0.9), self.buffer_roll_limit/self.bufferlen


	def set_search_map(self, f_center_search_map):
		set_f_center_search_map(statemx=self.FFTstatemx, search_map=f_center_search_map)


	def switch_baudrate(self, baudrate, sps):
		assert type(sps) == int
		assert sps > 1
		self.config.baudrate = baudrate
		self.config.sps = sps
		self.config.check_validity()
		self._setup()


	def push_samples(self, batch, give_bits=False):
		ret = list()
		bits = np.zeros(0, dtype=np.int64)

		t0 = time.perf_counter()
		rs_head_new = resampler_execute_stream(in_arr=batch, ii0=0, nsamples=len(batch), out_arr=self.rs_array, io0=self.rs_head, statemx=self.resampler_statemx)
		self.dt_array[0] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		center_f_head_new, demodulation_head_new = fft_detect_and_freq_determ(sample_arr=self.rs_array, isample0=self.rs_head, nsamples=rs_head_new - self.rs_head,
																				center_f_arr=self.center_f_array, center_f_head0=self.center_f_head,
																				statemx=self.FFTstatemx, instr_arr=self.fft_instr_array)
		self.dt_array[1] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		dmdsynch_head_new, bit_head_new = demodulation_sequence(rs_arr=self.rs_array, centerf_arr=self.center_f_array, i_rs0=self.demodulation_head,
																nsamples=demodulation_head_new - self.demodulation_head, dmd_arr=self.dmd_array, synch_arr=self.synch_array,
																dmdsynch_head0=self.dmdsynch_head, JPLstatemx=self.JPLstatemx, demodmx=self.DSDstatemx,
																bitarr=self.bit_array, bitfarr=self.bit_f_array, bit_head0=0)
		self.dt_array[2] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		if bit_head_new > 0:
			bits = self.bit_array[:bit_head_new]
			bit_freqs = self.bit_f_array[:bit_head_new]
			if not give_bits:
				bits = np.clip(bits, 0, 1)
				payloads, payload_delimits, payload_frequencies = deframe(bits=bits, bit_frequencies=bit_freqs, deframer_mx=self.deframermx, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg)
				if len(payload_delimits) > 0:
					ret = self._split_payloads(payloads=payloads, delimits=payload_delimits, frequencies=payload_frequencies)
		self.dt_array[3] += (time.perf_counter() - t0)

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
		self.dt_array[4] += (time.perf_counter() - t0)

		if give_bits:
			return bits
		return ret


	def _split_payloads(self, payloads, delimits, frequencies):
		pl_list = list()
		for i_pl, (i0,i1) in enumerate(delimits):
			pl_list.append((bytes(payloads[i0:i1]), frequencies[i_pl] * self.config.sps * self.config.baudrate))
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
def get_a_precompiling_sampleset(rx_config:ReceiverConfig, do_print=False):
	sr0 = rx_config.sr0
	baudrate = rx_config.baudrate
	BT = rx_config.BT

	rel_offset_raw = (rx_config.f_center-rx_config.f_tune) / sr0  #0.1 * (sps*baudrate/sr0)
	rs_mx, rs_cfg = get_default_rs()
	pl = np.random.randint(0,255, RS_MAX_PL_LEN-2)
	bitstring = frame_packet(pl=pl, synchword_int=DEFAULT_SYNCHWORD, synchword_len=DEFAULT_SYNCHWORD_LEN, use_scrambler=True, use_rs=True, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=True)
	transmission = make_samples(sps_f=sr0/baudrate, bitstring=bitstring, f_offset=rel_offset_raw, power=1.0, mod_index=rx_config.mod_index, shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=301, n_silence_start=0, n_silence_end=0)

	n_fft_calibration = int(1.3 * rx_config.sr0 * (rx_config.fftlen*0.5) * (1 / rx_config.c_stat_update) / (rx_config.baudrate*rx_config.sps))
	nsamples = int(n_fft_calibration + len(transmission) + 1.0*sr0)
	i0 = int(n_fft_calibration)
	if do_print:
		print("\tEquivalent times per sample segment:")
		print("\t\t{} s for fft-calibration".format( round( n_fft_calibration/sr0 , 3 ) ))
		print("\t\t{} s for transmission".format( round( len(transmission)/sr0, 3)))
		print("\t\t{} s for margins".format( round(2.0, 3) ))
	samples = radionoise(n=nsamples, sr=sr0, W_per_Hz=0.01/baudrate)
	samples[i0:i0+len(transmission)] += transmission
	return samples


def precompile_receiver(rx_config:ReceiverConfig, do_print=False):
	if do_print:
		print("[Precompiling]")
	t0 = time.perf_counter()
	if do_print:
		print("\t[Generating sampleset]")
	samples = get_a_precompiling_sampleset(rx_config, do_print)
	samples = np.array(samples, dtype=np.complex128)
	t1 = time.perf_counter()
	nsamples = len(samples)
	rx1 = Receiver(config=rx_config)
	rx2 = Receiver(config=rx_config)
	c = 0
	ret_pls1 = list()
	ret_pls2 = list()
	if do_print:
		print("\t[Processing]")
	while c < nsamples:
		c2 = min(c + rx_config.batch_maxlen // 2, nsamples)
		batch = samples[c:c2]
		ret1 = rx1.push_samples(batch=batch, give_bits=False)
		ret2 = rx2.push_samples(batch=np.complex64(batch), give_bits=False)
		ret_pls1.extend(ret1)
		ret_pls2.extend(ret2)
		c = c2
	t2 = time.perf_counter()
	assert len(ret_pls1) == 1, len(ret_pls1)
	assert len(ret_pls2) == 1, len(ret_pls2)
	if do_print:
		print("\t[Precompiled in {} s.  ({} s for samples)]".format( round(t2-t0, 3), round(t1-t0, 3)  ))
# PRECOMPILE RECEIVER ====================================================================================================








