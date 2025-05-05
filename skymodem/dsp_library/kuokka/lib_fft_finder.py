import numpy as np
from numba import njit
from .lib_tools import make_samples2, njit_objmode_fft


_fft_mask_dict = dict()


def construct_fft_mask(sps, mod_index, BT_rx_match, fftlen, masklen, nn):
	for key,val in _fft_mask_dict.items():
		k_sps, k_mod_idx, k_BT, k_fftlen, k_masklen, k_nn = key
		if (sps == k_sps) and (mod_index == k_mod_idx) and (BT_rx_match == k_BT) and (fftlen == k_fftlen) and (masklen == k_masklen) and (nn <= k_nn):
			return val
	assert (masklen % 2) == 1
	nbits = int((fftlen*6 + sps*3 +1) / sps) + 1
	fft_stack = np.zeros(fftlen, dtype=np.float64)
	n_stacked = 0
	while n_stacked < nn:
		bits = np.random.randint(0,2, nbits)*2 - 1
		samples, _ = make_samples2(sps_f=sps, bitstring=bits, f_offset=0.0, power=1.0, mod_index=mod_index, shaper_BT_prod=BT_rx_match)
		i0 = np.random.randint(1,int(sps*2))
		n_snippets = int((len(samples)-i0)/fftlen) -1
		assert n_snippets > 1
		for j in range(n_snippets):
			snippet = samples[i0+j*fftlen:i0+(j+1)*fftlen]
			fft = np.fft.fftshift( np.abs( np.fft.fft(snippet) ) )
			fft_stack += fft
			n_stacked += 1
	fft_stack = fft_stack / n_stacked
	mask = fft_stack[fftlen//2-masklen//2 : fftlen//2+masklen//2 +1]
	assert len(mask) == masklen, (len(mask),masklen)
	#mask = mask - np.min(mask)
	mask = mask / np.max(mask)
	#print("[Constructed fft mask in {} ms]".format(round(1e3*T_construct,1)))
	_fft_mask_dict[ (sps, mod_index, BT_rx_match, fftlen, masklen, nn) ] = mask
	return mask


def get_empiric_masklen(fftlen, sps): #TODO this should be a function of mod_idx and BT...
	return int(0.5 * 1.5 * fftlen / sps)*2 + 1


#@njit(cache=True)
def create_fft_f_centerer_statemx(fftlen, sps, f_center_search_map, mod_index, BT_rx_match, centering_delay_mpr, c_center_decay):
	assert fftlen >= 32
	assert len(f_center_search_map) == fftlen
	assert 0.5 < c_center_decay < 1.0
	masklen = get_empiric_masklen(fftlen=fftlen, sps=sps)
	jumplen = int(fftlen/2)
	statemx = np.zeros( (8,fftlen) , dtype=np.float64 )
	statemx[0,0]  = fftlen
	statemx[0,1]  = jumplen
	statemx[0,2]  = int(centering_delay_mpr * fftlen)
	statemx[0,3]  = int(masklen)
	statemx[0,4]  = c_center_decay
	statemx[0,10] = 0 		# idx  (this runs from (fftlen-jumplen) to fftlen-1 and then an fft is called)
	statemx[0,11] = -1.0	# f_center
	statemx[0,12] = 0.0		# corrmax (just for instrumentation purposes)
	statemx[1,:]  = np.fft.fftshift( np.fft.fftfreq(fftlen, d=1.0) ) # frequency table
	statemx[2,:]  = 0.0		# fft mask-correlation
	statemx[3,:]  = 0.0		# window (real) (the only reason this matrix would be complex...)
	statemx[4,:]  = 0.0		# window (imag) (the only reason this matrix would be complex...)
	statemx[5, 0:masklen]  	+= construct_fft_mask(sps=sps, mod_index=mod_index, BT_rx_match=BT_rx_match, fftlen=fftlen, masklen=masklen, nn=1000) # empiric mask
	statemx[6,:]  = f_center_search_map
	return statemx


def set_f_center_search_map(statemx, search_map):
	assert len(search_map) == statemx[0,0]
	assert len(search_map) == statemx.shape[1]
	statemx[6,:] = search_map
	statemx[2,:] = 0


@njit(cache=True)
def fft_f_centerer(sample_arr, isample0, nsamples, center_f_arr, center_f_head0, statemx, instr_arr):
	fftlen 			  	= int(statemx[0,0])
	jumplen 		  	= int(statemx[0,1])
	n_centering_delay 	= int(statemx[0,2])
	masklen 		  	= int(statemx[0,3])
	c_center_decay		= statemx[0,4]
	idx 				= int(statemx[0,10])
	f_center 			= statemx[0,11]
	corrmax 			= statemx[0,12]
	window 				= statemx[3,:] + 1j*statemx[4,:]
	corr_mask			= statemx[5,0:masklen]
	boolean_search_map  = statemx[6,:]

	center_f_head = center_f_head0
	for i_in in range(isample0, isample0 + nsamples):
		if idx >= 0:
			window[idx] = sample_arr[i_in]
		idx += 1
		if idx == fftlen:
			idx = fftlen - jumplen
			fft = np.abs(njit_objmode_fft(window))
			if jumplen < fftlen:
				window = np.roll(window, -jumplen)
			for i0 in range(fftlen -masklen +1):
				i_center = i0 + masklen//2
				if boolean_search_map[i_center] == 0:
					continue
				i1 = i0 + masklen
				assert (i0 >= 0) and (i0 <= (fftlen-masklen))
				assert (i1 >= masklen) and (i1 <= fftlen)
				statemx[2,i_center] = (statemx[2,i_center] + np.sum(fft[i0:i1] * corr_mask)) * c_center_decay #0.95
			argmax = np.argmax(statemx[2,:])
			corrmax = statemx[2,argmax]
			f_maxx = statemx[1,:][argmax]
			f_center = f_maxx

		if (center_f_head-n_centering_delay) >= 0:
			center_f_arr[center_f_head-n_centering_delay] = f_center
		instr_arr[center_f_head,0] = corrmax
		center_f_head += 1
	#assert center_f_head == center_f_head0 + nsamples
	statemx[0,10] = idx
	statemx[0,11] = f_center
	statemx[0,12] = corrmax
	statemx[3,:] = window.real
	statemx[4,:] = window.imag
	return center_f_head, max(0, center_f_head - (n_centering_delay + 1))   # demodulation head. (the demodulation stage should be given samples up to this head)




# === carrier sensed =========================================================================================================================================================================
def create_fft_f_centerer_csense_statemx(fftlen, sps, f_center_search_map, mod_index, BT_rx_match, centering_delay_mpr, c_center_decay, c_stat_update, carrier_sense_threshold):
	assert fftlen >= 32
	assert len(f_center_search_map) == fftlen
	assert 0.5 < c_center_decay < 1.0
	masklen = get_empiric_masklen(fftlen=fftlen, sps=sps)
	jumplen = int(fftlen/2)
	statemx = np.zeros( (8,fftlen) , dtype=np.float64 )
	statemx[0,0]  = fftlen
	statemx[0,1]  = jumplen
	statemx[0,2]  = int(centering_delay_mpr * fftlen)
	statemx[0,3]  = int(masklen)
	statemx[0,4]  = c_center_decay
	statemx[0,5]  = c_stat_update
	statemx[0,6]  = carrier_sense_threshold
	statemx[0,10] = 0 		# idx  (this runs from (fftlen-jumplen) to fftlen-1 and then an fft is called)
	statemx[0,11] = -1.0	# f_center
	statemx[0,12] = 0.0		# stat avg
	statemx[0,13] = 1.0		# stat std
	statemx[0,14] = 0.0		# n_stat_update
	statemx[0,15] = 0.0		# carrier_sensed
	statemx[0,16] = 0.0		# corrmax_smooth (just for instrumentation purposes)
	statemx[0,17] = 0.0		# corrmax        (just for instrumentation purposes)
	statemx[1,:]  = np.fft.fftshift( np.fft.fftfreq(fftlen, d=1.0) ) # frequency table
	statemx[2,:]  = 0.0		# fft mask-correlation
	statemx[3,:]  = 0.0		# window (real) (the only reason this matrix would be complex...)
	statemx[4,:]  = 0.0		# window (imag) (the only reason this matrix would be complex...)
	statemx[5, 0:masklen]  	+= construct_fft_mask(sps=sps, mod_index=mod_index, BT_rx_match=BT_rx_match, fftlen=fftlen, masklen=masklen, nn=1000) # empiric mask
	statemx[6,:]  = f_center_search_map
	return statemx



@njit(cache=True)
def fft_f_centerer_csense(sample_arr, isample0, nsamples, center_f_arr, center_f_head0, statemx, instr_arr):
	fftlen 			  	= int(statemx[0,0])
	jumplen 		  	= int(statemx[0,1])
	n_centering_delay 	= int(statemx[0,2])
	masklen 		  	= int(statemx[0,3])
	c_center_decay		= statemx[0,4]
	c_stat_update		= statemx[0,5]		 # for carrier sense
	carrier_sense_threshold = statemx[0,6]	 # for carrier sense
	idx 				= int(statemx[0,10])
	f_center 			= statemx[0,11]
	avg 				= statemx[0,12]		 # for carrier sense
	std 				= statemx[0,13]		 # for carrier sense
	n_stat_update 		= statemx[0,14]		 # for carrier sense
	carrier_sensed 		= int(statemx[0,15]) # for carrier sense
	corrmax_smooth 		= statemx[0,16]
	corrmax 			= statemx[0,17]		 # for carrier sense
	window 				= statemx[3,:] + 1j*statemx[4,:]
	corr_mask			= statemx[5,0:masklen]
	boolean_search_map  = statemx[6,:]

	center_f_head = center_f_head0
	corr_array = np.zeros(fftlen, dtype=np.float64)
	for i_in in range(isample0, isample0 + nsamples):
		if idx >= 0:
			window[idx] = sample_arr[i_in]
		idx += 1
		if idx == fftlen:
			idx = fftlen - jumplen
			fft = np.abs(njit_objmode_fft(window))
			if jumplen < fftlen:
				window = np.roll(window, -jumplen)
			for i0 in range(fftlen -masklen +1):
				i_center = i0 + masklen//2
				if boolean_search_map[i_center] == 0:
					continue
				i1 = i0 + masklen
				assert (i0 >= 0) and (i0 <= (fftlen-masklen))
				assert (i1 >= masklen) and (i1 <= fftlen)
				corr_array[i_center] = np.sum(fft[i0:i1] * corr_mask)
				#statemx[2,i_center] = (statemx[2,i_center] + np.sum(fft[i0:i1] * statemx[5,0:masklen])) * c_center_decay #0.95
			statemx[2,:] = (statemx[2,:] + corr_array) * c_center_decay
			argmax_corr_smooth = np.argmax(statemx[2,:])
			corrmax_smooth = statemx[2,argmax_corr_smooth]
			f_center = statemx[1,:][argmax_corr_smooth]
			corrmax = np.max(corr_array)	# for carrier sense
			avg, std = corr_stat_update(avg0=avg, std0=std, corrmax=corrmax, c_update=c_stat_update, n_update=n_stat_update) # for carrier sense
			n_stat_update += 1					# for carrier sense
			carrier_sensed = int(((corrmax-avg)/std) > carrier_sense_threshold) # for carrier sense

		if (center_f_head-n_centering_delay) >= 0:
			center_f_arr[center_f_head-n_centering_delay] = f_center
		instr_arr[center_f_head,0] = corrmax_smooth
		instr_arr[center_f_head,1] = corrmax		# for carrier sense
		instr_arr[center_f_head,2] = avg			# for carrier sense
		instr_arr[center_f_head,3] = std			# for carrier sense
		center_f_head += 1
	#assert center_f_head == center_f_head0 + nsamples
	statemx[0,10] = idx
	statemx[0,11] = f_center
	statemx[0,12] = avg				# for carrier sense
	statemx[0,13] = std				# for carrier sense
	statemx[0,14] = n_stat_update	# for carrier sense
	statemx[0,15] = carrier_sensed	# for carrier sense
	statemx[0,16] = corrmax_smooth
	statemx[0,17] = corrmax			# for carrier sense
	statemx[3,:] = window.real
	statemx[4,:] = window.imag
	return center_f_head, max(0, center_f_head - (n_centering_delay + 1)), carrier_sensed  # demodulation head. (the demodulation stage should be given samples up to this head)


@njit(cache=True)
def corr_stat_update(avg0, std0, corrmax, c_update, n_update):
	var0 = std0**2
	in_start = n_update < (1.0/c_update)
	if in_start:  # a strictly optimal implementation would skip the division and give this as a precomputed parameter...
		c_update = 1.0/(n_update+1.0)
	corrmax_use = corrmax
	if (not in_start) and (corrmax_use > (avg0+std0*5.5)):
		corrmax_use = avg0+std0*1.15 # implicitly assumes all 'corrmax' values (and avg) to be nonnegative. A general implementation would use a sign-agnostic deviation...
	avg1 = avg0 + c_update * (corrmax_use-avg0)
	var1 = var0 + c_update * (((corrmax_use-avg0)**2)-var0)
	if var1 <= 0:
		var1 = 1.0
	std1 = var1**0.5
	return avg1, std1


