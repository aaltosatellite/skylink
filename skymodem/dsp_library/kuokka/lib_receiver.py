import time

import numpy as np
#from matplotlib import pyplot as plt
from .lib_demodulation import demodulation_sequence, create_DSD_statemx, DSD_buffer_roll
from .lib_symsynching import create_classic_JPL_statemx
from .lib_fft_detector import fft_detect_and_freq_determ, create_fft_centering_statemx, get_center_frequency_estimate
from .lib_framing import create_deframer, deframe, RS_MAX_PL_LEN, RS_MAX_ENCODED_LEN
from .lib_tools import DEFAULT_SYNCHWORD
from .lib_reedsolomon import get_default_rs
from .lib_resampler import resampler_execute_stream, create_resampler


class ReceiverSettings:
	def __init__(self, sr0, baudrate, bufferlen, batch_maxlen, f_tune, f_expected):
		assert sr0 > (baudrate * 2)
		self.f_tune 			= f_tune		# tuned frequency of the radio in absolute Hz (for example 350.0e6)
		self.f_expected 		= f_expected	# expected frequency of the transmissions in absolute Hz (for example 350.12e6)
		self.sr0 				= sr0			# raw samplerate of the radio. Will be downsampled with a rate of baudrate*sps/sr0
		self.baudrate			= baudrate		# baudrate of the transmission
		self.bufferlen			= bufferlen
		self.batch_maxlen		= batch_maxlen
		# resampling ---------------------------------------
		self.sps 				= 21			# !			# sps (samples-per-symbol) for the signal processing pipeline. Determines resampling rate.
		self.m_halflen 			= 25			# !
		self.n_banks 			= 64
		self.rs_f_cutoff_coeff 	= 0.499						# determines lowpass associated with the resampling. In interval (0 : 0.5)
		# --------------------------------------------------
		# fft detection ------------------------------------
		self.fftlen 			= 1024			# ~
		self.jumplen 			= 512			# ~
		self.mod_index 			= 0.5			# ~
		self.BT 				= -1			# ~
		self.c_stat_update 		= 1/700.0		# ~ D-vs-c
		self.c_f_update_minimum = 0.025			# ~ D-vs-c
		self.T_f_upd_recovery 	= 2.0			# ~ D-vs-c
		self.fft_trigger_on_level 	= 4.8		# !!
		self.fft_trigger_off_level 	= 3.0		# !!
		self.mask_mode 			= 1	# !
		self.start_margin_mpr 	= 1.0			# !
		self.end_margin_mpr 	= 0.5			# ~
		# --------------------------------------------------
		# JPL synchronizer ---------------------------------
		self.JPL_n_decay 		= 28.0			# ! D-vs-c
		# --------------------------------------------------
		# demodulation -------------------------------------
		self.lp_ntaps			= 161			# ~
		self.lp_cutoff_coeff	= 0.625			# !
		self.synch_delay_mpr	= 22.0			# !
		# --------------------------------------------------
		# framing ------------------------------------------
		self.use_scrambler 		= True
		self.use_rs 			= True
		self.synch_threshold 	= 3
		self.data_maxlen 		= RS_MAX_ENCODED_LEN
		# --------------------------------------------------

	def check_validity(self):
		if not ((self.f_tune == -1) and (self.f_expected == -1)):
			assert self.f_tune >= 0
			assert self.f_expected >= 0
			assert abs(self.f_tune - self.f_expected) < (0.5 * self.sps * self.baudrate)
			fft_df = (self.sps * self.baudrate) / self.fftlen
			assert abs(self.f_tune - self.f_expected) < ((0.5 * self.sps * self.baudrate) - ((self.get_masklen()//2)*fft_df))
			assert (abs(self.f_tune - self.f_expected) + (0.5*self.baudrate*max(self.mod_index,1))) < (self.rs_f_cutoff_coeff*self.sps*self.baudrate)
		assert 1e3 < self.sr0 < 12e6
		assert 0 < self.baudrate < (self.sr0/2)
		assert 1000 < self.bufferlen < 100e6
		assert type(self.bufferlen) == int
		assert 10 < self.batch_maxlen < (0.1*self.bufferlen)
		assert type(self.batch_maxlen) == int
		assert 1 < self.sps < (3.9 * self.sr0 / self.baudrate)
		assert type(self.sps) == int
		assert 5 < self.m_halflen < 42
		assert type(self.m_halflen) == int
		assert 24 < self.n_banks < 240
		assert type(self.n_banks) == int
		assert 0 < self.rs_f_cutoff_coeff < 0.5
		assert self.fftlen in (256, 512, 1024, 2048)
		assert type(self.fftlen) == int
		assert 1 < self.jumplen < (2*self.fftlen)
		assert type(self.jumplen) == int
		assert 0.5 <= self.mod_index < 10.0
		assert type(self.BT) in (float, int)
		assert (self.BT >= 0.5) or (self.BT == -1)
		assert 0 < self.c_stat_update <= 1.0
		assert 0 < self.c_f_update_minimum <= 1.0
		assert 0 < self.T_f_upd_recovery < 1800
		assert -1.0 <= self.fft_trigger_on_level <= 9.0
		assert -1.0 <= self.fft_trigger_off_level <= 9.0
		assert self.fft_trigger_off_level <= self.fft_trigger_on_level
		assert self.mask_mode in (0,1)
		assert 0 <= self.start_margin_mpr < 100.0  #was 10
		assert 0 <= self.end_margin_mpr < 100.0     # was 5
		assert 1.0 <= self.JPL_n_decay < 100.0
		assert 30.0 <= self.lp_ntaps < 300.0
		assert type(self.lp_ntaps) == int
		assert (self.lp_ntaps % 2) == 1
		assert 0 < self.lp_cutoff_coeff < (0.5*self.sps)
		assert 0 <= self.synch_delay_mpr <= 64.0


	def get_r_rate(self):
		r_rate = self.sps * self.baudrate / self.sr0
		return r_rate

	def get_masklen(self):
		if self.mask_mode == 0:
			return int(0.5 * self.fftlen / self.sps)*2 + 1  #TODO the best centering correlator should really be researched...
		return int(0.5 * 2.5 * self.fftlen / self.sps)*2 + 1  #TODO the best centering correlator should really be researched...


	def get_lp_cutoff(self):
		# lp_cutoff_coeff * baudrate / sr
		# = lp_cutoff_coeff * baudrate / (baudrate * sps)
		# = lp_cutoff_coeff / sps
		return self.lp_cutoff_coeff / self.sps

	def get_search_space_triplet(self):  #returns (f_tune, minimum_possible_center_frequency, maximum_possible_center_frequency)
		if (self.f_tune == -1) and (self.f_expected == -1):
			return -1, -1, -1
		df_doppler = self.f_expected * (((3e8+7500)/3e8) - 1)  # approximate maximum doppler shift for LEO orbital speed
		df_search_sideband = df_doppler * 1.2
		triplet = self.f_tune, self.f_expected - df_search_sideband, self.f_expected + df_search_sideband
		print("+[search space: {} MHz  -  {} MHz]".format( round(triplet[1]*1e-6, 3), round(triplet[2]*1e-6, 3) ))
		return triplet













class Receiver:
	def __init__(self, settings:ReceiverSettings):
		settings.check_validity()
		self.settings = settings
		self.bufferlen 			= int(settings.bufferlen)
		self.rs_array 			= np.zeros(self.bufferlen, dtype=np.complex128)
		self.center_f_array 	= np.zeros(self.bufferlen, dtype=np.float64)
		self.dmd_array 			= np.zeros(self.bufferlen, dtype=np.float64)
		self.synch_array 		= np.zeros((self.bufferlen, 3), dtype=np.int64)
		self.bit_array 			= np.zeros(self.bufferlen // 10, dtype=np.int64)
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
		self.DPDstatemx 		= np.zeros((2,2), dtype=np.float64)
		self.deframermx 		= np.zeros((2,2), dtype=np.float64)
		rs_mx, rs_cfg 			= get_default_rs()
		self.rs_mx 				= rs_mx
		self.rs_cfg 			= rs_cfg
		self.dt_array			= np.zeros(5, dtype=np.float64)
		self._setup()
		# This series of baudrate switches pre-generates correlation masks to memory.
		_br = self.settings.baudrate
		_sps = self.settings.sps
		self.switch_baudrate(baudrate=9600, sps=_sps)
		self.switch_baudrate(baudrate=9600*2, sps=_sps)
		self.switch_baudrate(baudrate=9600*2*2, sps=_sps)
		#self.switch_baudrate(baudrate=4800, sps=_sps)
		self.switch_baudrate(baudrate=_br, sps=_sps)


	def _setup(self):
		# r_rate = settings.sps * settings.baudrate / settings.sr0
		settings = self.settings
		f_cutoff = settings.get_r_rate() * settings.rs_f_cutoff_coeff
		self.resampler_statemx = create_resampler(m_halflen=settings.m_halflen, n_banks=settings.n_banks, r_rate=settings.get_r_rate(), f_cutoff=f_cutoff, allow_aliasing=False)
		self.FFTstatemx = create_fft_centering_statemx(fftlen=settings.fftlen, jumplen=settings.jumplen, sps=settings.sps, baudrate=settings.baudrate,
													   search_space_triplet=settings.get_search_space_triplet(), mod_index=settings.mod_index,
													   BT=settings.BT, c_stat_update=settings.c_stat_update, c_f_update_minimum=settings.c_f_update_minimum,
													   T_f_upd_recovery=settings.T_f_upd_recovery, fft_trigger_on_level=settings.fft_trigger_on_level,
													   fft_trigger_off_level=settings.fft_trigger_off_level, masklen=settings.get_masklen(), avg0=0.0, var0=1.0,
													   mask_mode=settings.mask_mode, start_margin_mpr=settings.start_margin_mpr, end_margin_mpr=settings.end_margin_mpr)
		self.JPLstatemx = create_classic_JPL_statemx(N_eps=settings.sps, n_decay=settings.JPL_n_decay)
		self.DPDstatemx = create_DSD_statemx(lp_ntaps=settings.lp_ntaps, lp_cutoff=settings.get_lp_cutoff(), synch_delay_mpr_f=settings.synch_delay_mpr, sps_f=settings.sps, f_center=0)
		self.deframermx = create_deframer(use_scrambler=settings.use_scrambler, use_rs=settings.use_rs, data_maxlen=settings.data_maxlen,
										  synchword=DEFAULT_SYNCHWORD, synchword_len=32, synch_threshold=settings.synch_threshold)
		a = int(settings.batch_maxlen * settings.get_r_rate() * 2)
		b = int((settings.start_margin_mpr + settings.end_margin_mpr) * (settings.fftlen+settings.jumplen))
		self.buffer_roll_limit 	= self.bufferlen - (a + b + 4)
		assert self.buffer_roll_limit > (self.bufferlen * 0.9), self.buffer_roll_limit/self.bufferlen


	def switch_baudrate(self, baudrate, sps):
		assert type(sps) == int
		assert sps > 1
		self.settings.baudrate = baudrate
		self.settings.sps = sps
		self.settings.check_validity()
		self._setup()


	def center_frequency_estimate(self):
		f_center, is_active = get_center_frequency_estimate(self.FFTstatemx, f_tune=self.settings.f_tune)
		print("(is active: {}), (c_f_update_long: {})".format(is_active,  self.FFTstatemx[0,27]))
		if is_active:
			return f_center
		return self.settings.f_expected


	def _split_payloads(self, payloads, delimits):
		pl_list = list()
		for (i0,i1) in delimits:
			pl_list.append( bytes(payloads[i0:i1]) )
			assert len(pl_list[-1]) == (i1-i0)
		return pl_list


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
																dmdsynch_head0=self.dmdsynch_head, JPLstatemx=self.JPLstatemx, demodmx=self.DPDstatemx, bitarr=self.bit_array, bit_head0=0)
		self.dt_array[2] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		if bit_head_new > 0:
			bits = self.bit_array[:bit_head_new]
			#print("got bits", bits[0:4])
			if not give_bits:
				bits = np.clip(bits, 0, 1)
				payloads, payload_delimits = deframe(bits=bits, deframer_mx=self.deframermx, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg)
				if len(payload_delimits) > 0:
					ret = self._split_payloads(payloads=payloads, delimits=payload_delimits)
		self.dt_array[3] += (time.perf_counter() - t0)

		t0 = time.perf_counter()
		self.rs_head = rs_head_new
		self.center_f_head = center_f_head_new
		self.demodulation_head = demodulation_head_new
		self.dmdsynch_head = dmdsynch_head_new
		assert self.rs_head == self.center_f_head
		if self.rs_head >= self.buffer_roll_limit:
			self._buffer_roll_1()
			#self._buffer_roll_2()
		if self.dmdsynch_head >= self.buffer_roll_limit:
			#self._buffer_roll_1()
			self._buffer_roll_2()
		self.dt_array[4] += (time.perf_counter() - t0)

		if give_bits:
			return bits
		return ret


	def _buffer_roll_1(self):
		#print("ROLL 1")
		self.rs_array[0:self.rs_head-self.bufferhalf] 				= self.rs_array[self.bufferhalf:self.rs_head]
		self.center_f_array[0:self.center_f_head-self.bufferhalf] 	= self.center_f_array[self.bufferhalf:self.center_f_head]
		self.fft_instr_array[0:self.center_f_head-self.bufferhalf] 	= self.fft_instr_array[self.bufferhalf:self.center_f_head]
		self.rs_head 			= self.rs_head - self.bufferhalf
		self.center_f_head 		= self.center_f_head - self.bufferhalf
		#self.demodulation_head 	= max(0,self.demodulation_head - self.bufferhalf)
		self.demodulation_head 	= self.demodulation_head - self.bufferhalf


	def _buffer_roll_2(self):
		#print("ROLL 2")
		self.dmd_array[0:self.dmdsynch_head-self.bufferhalf] 		= self.dmd_array[self.bufferhalf:self.dmdsynch_head]
		self.synch_array[0:self.dmdsynch_head-self.bufferhalf] 		= self.synch_array[self.bufferhalf:self.dmdsynch_head]
		self.dmdsynch_head = self.dmdsynch_head - self.bufferhalf
		DSD_buffer_roll(demodmx=self.DPDstatemx, buffers_receded_by=self.bufferhalf)


























