import numpy as np
from numba import njit
from .lib_symsynching import classic_JPL_synch_strm, classic_JPL_synch_reset
from .lib_decider import symbol_decision
from scipy.signal import firwin



@njit(cache=True)
def demodulation_sequence(rs_arr, centerf_arr, i_rs0, nsamples, dmd_arr, synch_arr, dmdsynch_head0, JPLstatemx, demodmx, bitarr, bitfarr, bit_head0):
	bit_head = bit_head0
	i_start = -1
	dmdsynch_head = dmdsynch_head0
	# a demodulation process is ongoing. (Is this a reliable indicator??? Should be...)
	if JPLstatemx[0,1] > 0:
		i_start = i_rs0
	for i_rs in range(i_rs0, i_rs0 + nsamples):

		# an ongoing transmission ends.
		if (i_start >= 0) and (centerf_arr[i_rs] < -0.5):
			#dmdsynch_head, bits = demod_synch_decide(rs_arr=rs_arr, i_rs0=i_start, nsamples=i_rs-i_start, dmd_arr=dmd_arr, synch_arr=synch_arr, dmdsynch_head0=dmdsynch_head, JPLstatemx=JPLstatemx, demodmx=demodmx)
			dmdsynch_head, bits, bit_frequencies = demod_synch_decide_cont_f(rs_arr=rs_arr, center_f_arr=centerf_arr, i_rs0=i_start, nsamples=i_rs-i_start, dmd_arr=dmd_arr, synch_arr=synch_arr, dmdsynch_head0=dmdsynch_head, JPLstatemx=JPLstatemx, demodmx=demodmx)
			bitarr[bit_head:bit_head+len(bits)] = bits
			bitfarr[bit_head:bit_head+len(bits)] = bit_frequencies
			bit_head += len(bits)
			classic_JPL_synch_reset(JPLstatemx)
			DSD_reset(demodmx=demodmx, f_center=0.0)  # unnecessary, since demodmx has to be reset in the beginning with the correct frequency anyway. But this ties up loose ends.
			i_start = -1

		# a transmission begins. Demodulation will be called either when it ends, or if the current chunk ends before that.
		if (i_start < 0) and (centerf_arr[i_rs] > -0.5):
			DSD_reset(demodmx=demodmx, f_center=centerf_arr[i_rs])
			i_start = i_rs

	# chunk ends with demodulation on. Demodulate to the end.
	if i_start >= 0:
		#dmdsynch_head, bits = demod_synch_decide(rs_arr=rs_arr, i_rs0=i_start, nsamples=i_rs0+nsamples-i_start, dmd_arr=dmd_arr, synch_arr=synch_arr, dmdsynch_head0=dmdsynch_head, JPLstatemx=JPLstatemx, demodmx=demodmx)
		dmdsynch_head, bits, bit_frequencies = demod_synch_decide_cont_f(rs_arr=rs_arr, center_f_arr=centerf_arr, i_rs0=i_start, nsamples=i_rs0+nsamples-i_start, dmd_arr=dmd_arr, synch_arr=synch_arr, dmdsynch_head0=dmdsynch_head, JPLstatemx=JPLstatemx, demodmx=demodmx)
		bitarr[bit_head:bit_head+len(bits)] = bits
		bitfarr[bit_head:bit_head+len(bits)] = bit_frequencies
		bit_head += len(bits)

	return dmdsynch_head, bit_head





@njit(cache=True, parallel=False)
def DSD_reset(demodmx, f_center):
	demodmx[0,0] = 0.0			# f_shift phase 			(not mandatory)
	demodmx[0,1] = 1.0			# lowpass previous sample 	(not mandatory)
	demodmx[0,2] = 0.0			# lowpass phase 			(not mandatory)
	demodmx[0,3] = f_center		# center frequency
	demodmx[0,4] = -1.0			# last_symbol_idx_f
	demodmx[1,:] = 0.0			# lowpass window 			(not mandatory)

@njit(cache=True, parallel=False)
def DSD_buffer_roll(demodmx, buffers_receded_by):
	assert buffers_receded_by >= 0
	demodmx[0,4] = demodmx[0,4] - buffers_receded_by



def create_DSD_statemx(lp_ntaps, lp_cutoff, synch_delay_mpr_f, sps_f, f_center):
	assert lp_ntaps > 10
	assert (lp_ntaps % 2) == 1
	assert abs(f_center) < 0.5
	assert lp_cutoff > 0
	assert lp_cutoff < 0.5
	assert synch_delay_mpr_f >= 0
	assert sps_f >= 1
	lp_taps = firwin(numtaps=lp_ntaps, cutoff=lp_cutoff, fs=1.0, pass_zero=True)
	demodmx = np.zeros( (2+len(lp_taps), len(lp_taps)), dtype=np.complex128 )
	demodmx[0,0] = 0.0							# f_shift phase
	demodmx[0,1] = 1.0							# lowpass previous sample
	demodmx[0,2] = 0.0							# lowpass phase
	demodmx[0,3] = f_center						# center frequency
	demodmx[0,4] = -1.0							# last_symbol_idx_f
	demodmx[0,5] = int(synch_delay_mpr_f*sps_f)	# synch delay
	demodmx[0,6] = sps_f						# (expected) sps as a float
	demodmx[1,:] = 0.0							# lowpass window
	for itap in range(lp_ntaps):
		demodmx[itap+2,:] = np.roll(lp_taps, (itap+1) % lp_ntaps)		# lowpass filter taps in different phases
	return demodmx



@njit(cache=True, parallel=False)
def DSD_set_f_center(demodmx, f_center):
	demodmx[0,3] = f_center





@njit(cache=True, parallel=False)
def demod_synch_decide(rs_arr, i_rs0, nsamples, dmd_arr, synch_arr, dmdsynch_head0, JPLstatemx, demodmx):
	assert np.iscomplexobj(demodmx)
	#assert type(synch_arr[0,0]) is int64
	#assert len(dmd_arr.shape) == 1
	assert demodmx.shape[0] == demodmx.shape[1]+2
	shift_phase 	= 1.0j * demodmx[0,0].imag
	s_lpd_prev 		= demodmx[0,1]
	lp_tap_phase 	= int(demodmx[0,2].real)
	f_center		= demodmx[0,3].real
	lp_taps 		= demodmx[2:,:]
	ntaps 			= len(demodmx[0])
	assert abs(f_center) < 0.5
	dmd_head = dmdsynch_head0
	for i_rs in range(i_rs0, i_rs0 + nsamples):
		s_shifted = rs_arr[i_rs] * np.exp(shift_phase)
		shift_phase -= 2j*np.pi*f_center
		demodmx[1][lp_tap_phase] = s_shifted
		s_lpd = 0.0
		for jj in range(ntaps):
			s_lpd += demodmx[1][jj] * lp_taps[lp_tap_phase][jj]
		zz = s_lpd * np.conj(s_lpd_prev)
		s_lpd_prev = s_lpd
		dmd_arr[dmd_head] = np.arctan2(zz.imag, zz.real)
		dmd_head += 1
		lp_tap_phase = (lp_tap_phase+1) % ntaps
	demodmx[0,0] = shift_phase
	demodmx[0,1] = s_lpd_prev
	demodmx[0,2] = lp_tap_phase

	synch_head = classic_JPL_synch_strm(sample_arr=dmd_arr, i_sample0=dmdsynch_head0, nsamples=nsamples, synch_arr=synch_arr, synch_head0=dmdsynch_head0, statemx=JPLstatemx)
	assert synch_head == dmd_head

	#last_symbol_absidx_f
	dmd_opt_idx_f 	= demodmx[0,4].real
	sps_f 			= demodmx[0,6].real
	synch_delay_i 	= int(demodmx[0,5].real)  # TODO this should decay towards the end. 1) enables reception of last symbols, and 2) prevents loss of synch for last symbols.
	Neps 			= JPLstatemx.shape[1]
	assert synch_delay_i > 0
	if dmd_opt_idx_f < 0:					# first call (after reset). measurement index is set to the beginning of dmd_array (which is at io0)
		dmd_opt_idx_f = dmdsynch_head0
	else:
		assert dmd_opt_idx_f <= max(0, dmdsynch_head0 - 1)
	bitarr = np.zeros(int(1.5*nsamples/sps_f)+3, dtype=np.int64)
	bit_f_arr = np.zeros(int(1.5*nsamples/sps_f)+3, dtype=np.float64)
	ibit = 0
	while True:
		vsym, i_tap = symbol_decision(dmd_arr=dmd_arr, synch_arr=synch_arr, dmd_synch_head_i=dmd_head, prev_dmd_idx_f=dmd_opt_idx_f, sps_f=sps_f, synch_delay_i=synch_delay_i, N_eps_i=Neps)
		if i_tap < 0:
			break
		dmd_opt_idx_f = i_tap
		bitarr[ibit] = np.sign(vsym)
		bit_f_arr[ibit] = f_center
		assert bit_f_arr[ibit] > -0.5
		ibit += 1
	demodmx[0,4] = dmd_opt_idx_f

	#dmd_arr[0:len(dmd_arr)-nsamples] = dmd_arr[nsamples:len(dmd_arr)]
	#synch_arr[0:len(synch_arr)-nsamples] = synch_arr[nsamples:len(synch_arr)]
	return dmd_head, bitarr[0:ibit], bit_f_arr[0:ibit]










@njit(cache=True, parallel=False)
def demod_synch_decide_cont_f(rs_arr, center_f_arr, i_rs0, nsamples, dmd_arr, synch_arr, dmdsynch_head0, JPLstatemx, demodmx):
	assert np.iscomplexobj(demodmx)
	#assert type(synch_arr[0,0]) is int64
	#assert len(dmd_arr.shape) == 1
	assert demodmx.shape[0] == demodmx.shape[1]+2
	shift_phase 	= 1.0j * demodmx[0,0].imag
	s_lpd_prev 		= demodmx[0,1]
	lp_tap_phase 	= int(demodmx[0,2].real)
	#f_center		= demodmx[0,3].real
	lp_taps 		= demodmx[2:,:]
	ntaps 			= len(demodmx[0])
	dmd_head = dmdsynch_head0
	for i_rs in range(i_rs0, i_rs0 + nsamples):
		s_shifted = rs_arr[i_rs] * np.exp(shift_phase)
		shift_phase -= 2j*np.pi*center_f_arr[i_rs]
		demodmx[1][lp_tap_phase] = s_shifted
		s_lpd = 0.0
		for jj in range(ntaps):
			s_lpd += demodmx[1][jj] * lp_taps[lp_tap_phase][jj]
		zz = s_lpd * np.conj(s_lpd_prev)
		s_lpd_prev = s_lpd
		dmd_arr[dmd_head] = np.arctan2(zz.imag, zz.real)
		dmd_head += 1
		lp_tap_phase = (lp_tap_phase+1) % ntaps
	demodmx[0,0] = shift_phase
	demodmx[0,1] = s_lpd_prev
	demodmx[0,2] = lp_tap_phase

	synch_head = classic_JPL_synch_strm(sample_arr=dmd_arr, i_sample0=dmdsynch_head0, nsamples=nsamples, synch_arr=synch_arr, synch_head0=dmdsynch_head0, statemx=JPLstatemx)
	assert synch_head == dmd_head

	#last_symbol_absidx_f
	dmd_opt_idx_f 	= demodmx[0,4].real
	sps_f 			= demodmx[0,6].real
	synch_delay_i 	= int(demodmx[0,5].real)  # TODO this should decay towards the end. 1) enables reception of last symbols, and 2) prevents loss of synch for last symbols.
	Neps 			= JPLstatemx.shape[1]
	assert synch_delay_i >= 0
	if dmd_opt_idx_f < 0:					# first call (after reset). measurement index is set to the beginning of dmd_array (which is at io0)
		dmd_opt_idx_f = dmdsynch_head0
	else:
		assert dmd_opt_idx_f <= max(0, dmdsynch_head0 - 1)
	bitarr = np.zeros(int(1.5*nsamples/sps_f)+3, dtype=np.int64)
	bit_f_arr = np.zeros(int(1.5*nsamples/sps_f)+3, dtype=np.float64)
	ibit = 0
	while True:
		vsym, i_tap = symbol_decision(dmd_arr=dmd_arr, synch_arr=synch_arr, dmd_synch_head_i=dmd_head, prev_dmd_idx_f=dmd_opt_idx_f, sps_f=sps_f, synch_delay_i=synch_delay_i, N_eps_i=Neps)
		if i_tap < 0:
			break
		dmd_opt_idx_f = i_tap
		bitarr[ibit] = np.sign(vsym)
		bit_f_arr[ibit] = center_f_arr[i_rs0 + (int(round(i_tap))-dmdsynch_head0)] #int(round(i_tap))
		assert bit_f_arr[ibit] > -0.5
		ibit += 1
	demodmx[0,4] = dmd_opt_idx_f

	#dmd_arr[0:len(dmd_arr)-nsamples] = dmd_arr[nsamples:len(dmd_arr)]
	#synch_arr[0:len(synch_arr)-nsamples] = synch_arr[nsamples:len(synch_arr)]
	return dmd_head, bitarr[0:ibit], bit_f_arr[0:ibit]


