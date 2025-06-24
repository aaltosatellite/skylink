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
	power_band_length = int(0.5 * 1.1 * (mod_index/0.5) * fftlen / sps)*2 +1 # 0.75*baudrate is approximately the bandwidth of half max amplitude.		# for energy sense
	assert power_band_length <= masklen
	jumplen = int(fftlen/2)
	f_center_search_map_arr = np.array(f_center_search_map).copy()
	for i in range(fftlen):
		if (i < (masklen//2)) or (i >= (fftlen - (masklen//2))):
			f_center_search_map_arr[i] = 0
	search_center_indexes = np.array([i for i in range(len(f_center_search_map_arr)) if (f_center_search_map_arr[i]>0)])	# for energy sense
	assert np.all(search_center_indexes >= masklen//2)
	assert np.all(search_center_indexes < (fftlen-masklen//2))
	statemx = np.zeros( (8,fftlen) , dtype=np.float64 )
	statemx[0,0]  = fftlen
	statemx[0,1]  = jumplen
	statemx[0,2]  = int(centering_delay_mpr * fftlen)
	statemx[0,3]  = int(masklen)
	statemx[0,4]  = c_center_decay
	statemx[0,5]  = c_stat_update
	statemx[0,6]  = carrier_sense_threshold
	statemx[0,7]  = len(search_center_indexes)   																			# for energy sense
	statemx[0,8]  = power_band_length  																						# for energy sense
	statemx[0,10] = 0 		# idx  (this runs from (fftlen-jumplen) to fftlen-1 and then an fft is called)
	statemx[0,11] = -1.0	# f_center
	statemx[0,12] = 0.0		# max_corr avg
	statemx[0,13] = 1.0		# max_corr std
	statemx[0,14] = 0.0		# n_stat_update
	statemx[0,15] = 0.0		# carrier_sensed
	statemx[0,16] = 0.0		# corrmax_smooth (just for instrumentation purposes)
	statemx[0,17] = 0.0		# max_corr
	statemx[0,18] = 0.1		# band power avg																				# for energy sense
	statemx[0,19] = 1.0		# band power std																				# for energy sense
	statemx[0,20] = 0.0		# band power																					# for energy sense
	statemx[1,:]  = np.fft.fftshift( np.fft.fftfreq(fftlen, d=1.0) ) # frequency table
	statemx[2,:]  = 0.0		# fft mask-correlation
	statemx[3,:]  = 0.0		# window (real) (the only reason this matrix would be complex...)
	statemx[4,:]  = 0.0		# window (imag) (the only reason this matrix would be complex...)
	statemx[5, 0:masklen]  	+= construct_fft_mask(sps=sps, mod_index=mod_index, BT_rx_match=BT_rx_match, fftlen=fftlen, masklen=masklen, nn=1200) # empiric mask
	statemx[6,:]  = f_center_search_map_arr
	statemx[7,0:len(search_center_indexes)] = search_center_indexes 														# for energy sense
	return statemx



@njit(cache=True)
def fft_f_centerer_csense(sample_arr, isample0, nsamples, center_f_arr, power_arr, instr_arr, center_f_head0, statemx):
	fftlen 			  	= int(statemx[0,0])
	jumplen 		  	= int(statemx[0,1])
	n_centering_delay 	= int(statemx[0,2])
	masklen 		  	= int(statemx[0,3])
	c_center_decay		= statemx[0,4]
	c_stat_update		= statemx[0,5]			# for carrier sense
	carrier_sense_threshold = statemx[0,6]		# for carrier sense
	n_search_indexes	= int(statemx[0,7]) 	# for energy sense
	power_band_length	= int(statemx[0,8]) 	# for energy sense
	idx 				= int(statemx[0,10])
	f_center 			= statemx[0,11]
	max_corr_avg 		= statemx[0,12]			# for carrier sense
	max_corr_std 		= statemx[0,13]			# for carrier sense
	n_stat_update 		= statemx[0,14]			# for carrier sense
	carrier_sensed 		= int(statemx[0,15])	# for carrier sense
	max_expdec_corr 	= statemx[0,16]
	max_corr 			= statemx[0,17] 		# for carrier sense
	bp_avg 				= statemx[0,18] 		# for energy sense
	bp_std 				= statemx[0,19] 		# for energy sense
	band_power 			= statemx[0,20] 		# for energy sense
	window 				= statemx[3,:] + 1j*statemx[4,:]
	corr_mask			= statemx[5,0:masklen]
	boolean_search_map  = statemx[6,:]
	search_center_indexes = statemx[7,:] 		# for energy sense

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
			argmax_expdec_corr = np.argmax(statemx[2,:])
			max_expdec_corr = statemx[2,argmax_expdec_corr]
			f_center = statemx[1,:][argmax_expdec_corr]
			max_corr = np.max(corr_array)	# for carrier sense
			max_corr_avg, max_corr_std = stat_update(avg0=max_corr_avg, std0=max_corr_std, value=max_corr, c_update=c_stat_update, n_update=n_stat_update, clip_limit_instd=5.5, clip_replacement_instd=1.15) # for carrier sense
			n_stat_update += 1				# for carrier sense
			carrier_sensed = int(((max_corr-max_corr_avg)/max_corr_std) > carrier_sense_threshold) # for carrier sense
			# == Energy sense ================================================================================================================================================================
			if not carrier_sensed:
				i_a = search_center_indexes[np.random.randint(0,n_search_indexes)] - power_band_length//2  # random spot instead of argmax_expdec, to obtain non-biased average.
				i_b = i_a + power_band_length
				band_power = np.sum(fft[i_a:i_b]**2) / (fftlen**2)
				bp_avg, bp_std = stat_update(avg0=bp_avg, std0=bp_std, value=band_power, c_update=c_stat_update, n_update=n_stat_update, clip_limit_instd=5.5, clip_replacement_instd=1.15)
			else:
				i_a = argmax_expdec_corr - power_band_length//2
				i_b = i_a + power_band_length
				band_power = np.sum(fft[i_a:i_b]**2) / (fftlen**2)
			# == Energy sense ================================================================================================================================================================

		center_f_arr[max(0,center_f_head-n_centering_delay)] = f_center
		# == Energy sense ====================================================================================================================================================================
		power_arr[center_f_head,0] = band_power
		power_arr[center_f_head,1] = bp_avg
		power_arr[center_f_head,2] = power_band_length * (carrier_sensed*2-1)
		# == Energy sense ====================================================================================================================================================================
		instr_arr[center_f_head,0] = max_expdec_corr
		instr_arr[center_f_head,1] = max_corr			# for carrier sense
		instr_arr[center_f_head,2] = max_corr_avg		# for carrier sense
		instr_arr[center_f_head,3] = max_corr_std		# for carrier sense
		center_f_head += 1
	#assert center_f_head == center_f_head0 + nsamples
	statemx[0,10] = idx
	statemx[0,11] = f_center
	statemx[0,12] = max_corr_avg	# for carrier sense
	statemx[0,13] = max_corr_std	# for carrier sense
	statemx[0,14] = n_stat_update	# for carrier sense
	statemx[0,15] = carrier_sensed	# for carrier sense
	statemx[0,16] = max_expdec_corr
	statemx[0,17] = max_corr		# for carrier sense
	statemx[0,18] = bp_avg			# for energy sense
	statemx[0,19] = bp_std			# for energy sense
	statemx[0,20] = band_power		# for energy sense
	statemx[3,:] = window.real
	statemx[4,:] = window.imag
	dmd_up_to = max(0, center_f_head - (n_centering_delay + 1))
	return center_f_head, dmd_up_to, carrier_sensed  # demodulate_sample_arr_up_to. (the demodulation stage should be given samples up to this head)


@njit(cache=True)
def stat_update(avg0, std0, value, c_update, n_update, clip_limit_instd, clip_replacement_instd): # last to args usually about 5.5 and 1.15 ...
	var0 = std0**2
	in_start = n_update < (1.0/c_update)  # a strictly optimal implementation would skip the division and give this as a precomputed parameter...
	if in_start:
		c_update = 1.0/(n_update+1.0)
	value_use = value
	if (not in_start) and (np.abs(value_use-avg0) > (std0 * clip_limit_instd)):
		value_use = avg0 + std0 * clip_replacement_instd * np.sign(value_use - avg0)
	avg1 = avg0 + c_update * (value_use-avg0)
	var1 = var0 + c_update * (((value_use-avg0)**2)-var0)
	if var1 <= 0:
		var1 = 1.0
	std1 = var1**0.5
	return avg1, std1


