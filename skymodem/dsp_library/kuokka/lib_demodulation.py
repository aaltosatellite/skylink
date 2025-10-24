import numpy as np
from numba import njit
from .lib_decider import symbol_decision
from .lib_framing import deframe
from scipy.signal import firwin



def create_demod_statemx(lp_ntaps, lp_cutoff_coeff, sps_f):
	assert lp_ntaps > 10
	assert (lp_ntaps % 2) == 1
	assert (lp_cutoff_coeff/sps_f) > 0
	assert (lp_cutoff_coeff/sps_f) < 0.5
	assert sps_f >= 1
	lp_taps = firwin(numtaps=lp_ntaps, cutoff=lp_cutoff_coeff/sps_f, fs=1.0, pass_zero=True)
	demodmx = np.zeros( (2+len(lp_taps), len(lp_taps)), dtype=np.complex128 )
	demodmx[0,0] = 0.0							# f_shift phase
	demodmx[0,1] = 1.0							# lowpass previous sample
	demodmx[0,2] = 0.0							# lowpass phase
	demodmx[1,:] = 0.0							# lowpass window
	for itap in range(lp_ntaps):
		demodmx[itap+2,:] = np.roll(lp_taps, (itap+1) % lp_ntaps)		# lowpass filter taps in different phases
	return demodmx



@njit(cache=True, parallel=False)
def demodulate(sample_arr, center_f_arr, sample_i0, demod_n_samples, dmd_arr, demodmx):
	assert np.iscomplexobj(demodmx)
	assert demodmx.shape[0] == demodmx.shape[1]+2
	shift_phase 	= 1.0j * demodmx[0,0]
	s_lpd_prev 		= demodmx[0,1]
	lp_tap_phase 	= int(demodmx[0,2].real)
	lp_taps 		= demodmx[2:,:]
	ntaps 			= len(demodmx[0])
	for i in range(demod_n_samples):
		s_shifted = sample_arr[sample_i0+i] * np.exp(shift_phase)
		shift_phase -= 2j*np.pi*center_f_arr[sample_i0+i]
		demodmx[1][lp_tap_phase] = s_shifted
		s_lpd = 0.0
		for jj in range(ntaps):
			s_lpd += demodmx[1][jj] * lp_taps[lp_tap_phase][jj]
		zz = s_lpd * np.conj(s_lpd_prev)
		s_lpd_prev = s_lpd
		dmd_arr[sample_i0+i] = np.arctan2(zz.imag, zz.real)
		lp_tap_phase = (lp_tap_phase+1) % ntaps
	demodmx[0,0] = shift_phase.imag % (2*np.pi)
	demodmx[0,1] = s_lpd_prev
	demodmx[0,2] = lp_tap_phase
	dmd_head = sample_i0 + demod_n_samples
	return dmd_head



def create_DD_statemx(synch_delay_mpr_f, sps_f, Neps):
	assert synch_delay_mpr_f >= 0
	assert sps_f >= 1
	assert type(Neps) == int
	assert Neps >= 2
	sddmx = np.zeros( (2,4), dtype=np.float64)
	sddmx[0,0] = -1.0							# last_symbol_idx_f
	sddmx[0,1] = sps_f							# (expected) sps as a float
	sddmx[0,2] = int(synch_delay_mpr_f*sps_f)	# synch delay n
	sddmx[0,3] = Neps							# Neps of JPL synch.
	return sddmx



@njit(cache=True, parallel=False)
def decide_decode(dmd_arr, center_f_arr, power_arr, synch_arr, dmdsynch_head, opt_tap_idx_f0, sddmx, deframermx, rs_mx, rs_cfg):
	assert opt_tap_idx_f0 >= 0
	opt_tap_idx_f	= opt_tap_idx_f0
	sps_f 			= sddmx[0,1]
	synch_delay_i 	= int(sddmx[0,2])
	Neps 			= int(sddmx[0,3])
	assert synch_delay_i >= 0
	bitarr    = np.zeros(int(1.5*(dmdsynch_head - opt_tap_idx_f)/sps_f)+3, dtype=np.int64)
	bit_f_arr = np.zeros(int(1.5*(dmdsynch_head - opt_tap_idx_f)/sps_f)+3, dtype=np.float64)
	bit_p_arr = np.zeros((int(1.5*(dmdsynch_head - opt_tap_idx_f)/sps_f)+3, 3), dtype=np.float64) 		# for power sense
	ibit = 0
	while True:
		vsym, i_tap = symbol_decision(dmd_arr=dmd_arr, synch_arr=synch_arr, dmd_synch_head_i=dmdsynch_head, prev_dmd_idx_f=opt_tap_idx_f, sps_f=sps_f, synch_delay_i=synch_delay_i, N_eps_i=Neps)
		if i_tap < 0:
			break
		opt_tap_idx_f = i_tap
		bitarr[ibit] = np.sign(vsym)
		bit_f_arr[ibit] = center_f_arr[int(round(i_tap))] 		# int(round(i_tap))
		bit_p_arr[ibit] = power_arr[int(round(i_tap))]			# for power sense
		assert bit_f_arr[ibit] > -0.5
		ibit += 1
	bits = np.clip(bitarr[:ibit], 0, 1)
	payloads, payload_delimits, payload_frequencies, payload_powertuples, fault_counts = deframe(bits=bits, bit_frequencies=bit_f_arr[:ibit], bit_powers=bit_p_arr[:ibit], deframer_mx=deframermx, rs_mx=rs_mx, rs_cfg=rs_cfg)
	return payloads, payload_delimits, payload_frequencies, payload_powertuples, fault_counts, bitarr[:ibit], opt_tap_idx_f







