import numpy as np
from numba import njit, objmode
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
def create_cont_center_statemx(fftlen, sps, f_center_search_map, mod_index, BT_rx_match, centering_delay_mpr, c_center_decay):
	assert fftlen >= 32
	assert len(f_center_search_map) == fftlen
	assert 0.6 < c_center_decay < 1.0
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
def fft_continuous_f_center(sample_arr, isample0, nsamples, center_f_arr, center_f_head0, statemx, instr_arr):
	fftlen 			  	= int(statemx[0,0])
	jumplen 		  	= int(statemx[0,1])
	n_centering_delay 	= int(statemx[0,2])
	masklen 		  	= int(statemx[0,3])
	c_center_decay		= statemx[0,4]
	idx 				= int(statemx[0,10])
	f_center 			= statemx[0,11]
	corrmax 			= statemx[0,12]
	window 				= statemx[3,:] + 1j*statemx[4,:]
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
				statemx[2,i_center] = (statemx[2,i_center] + np.sum(fft[i0:i1] * statemx[5,0:masklen])) * c_center_decay #0.95
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




