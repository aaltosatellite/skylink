import numpy as np
from numba import njit, objmode
from .lib_tools import make_samples
import time

_fft_mask_dict = dict()



def construct_fft_mask(sps, mod_index, BT, fftlen, masklen, nn):
	for key,val in _fft_mask_dict.items():
		k_sps, k_mod_idx, k_BT, k_fftlen, k_masklen, k_nn = key
		if (sps == k_sps) and (mod_index == k_mod_idx) and (BT == k_BT) and (fftlen == k_fftlen) and (masklen == k_masklen) and (nn <= k_nn):
			return val

	assert (masklen % 2) == 1
	nbits = int((fftlen*6 + sps*3 +1) / sps) + 1
	t0 = time.perf_counter()
	mask0 = np.zeros(fftlen, dtype=np.float64)
	nloops = 1 + nn//6
	for _ in range(nloops):
		bits = np.random.randint(0,2, nbits)*2 - 1
		samples = make_samples(sps_f=sps, bitstring=bits, f_offset=0.0, power=1.0, mod_index=mod_index, shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=8*sps+1)
		i0 = np.random.randint(1,int(sps*2))
		assert len(samples) > (fftlen*6 + i0 + 1)
		snippet1 = samples[i0+fftlen*0:i0 + fftlen*1]
		snippet2 = samples[i0+fftlen*1:i0 + fftlen*2]
		snippet3 = samples[i0+fftlen*2:i0 + fftlen*3]
		snippet4 = samples[i0+fftlen*3:i0 + fftlen*4]
		snippet5 = samples[i0+fftlen*4:i0 + fftlen*5]
		snippet6 = samples[i0+fftlen*5:i0 + fftlen*6]
		fft1 = np.fft.fftshift( np.abs( np.fft.fft(snippet1) ) )
		fft2 = np.fft.fftshift( np.abs( np.fft.fft(snippet2) ) )
		fft3 = np.fft.fftshift( np.abs( np.fft.fft(snippet3) ) )
		fft4 = np.fft.fftshift( np.abs( np.fft.fft(snippet4) ) )
		fft5 = np.fft.fftshift( np.abs( np.fft.fft(snippet5) ) )
		fft6 = np.fft.fftshift( np.abs( np.fft.fft(snippet6) ) )
		mask0 += (fft1 + fft2 + fft3 + fft4 + fft5 + fft6)
	mask0 = mask0 / (6*nloops)
	mask = mask0[fftlen//2-masklen//2 : fftlen//2+masklen//2 +1]
	assert len(mask) == masklen, (len(mask),masklen)
	#mask = mask - np.min(mask)
	mask = mask / np.max(mask)
	T_construct = (time.perf_counter() - t0)
	print("Constructed fft mask in {} s".format(T_construct))
	_fft_mask_dict[ (sps, mod_index, BT, fftlen, masklen, nn) ] = mask
	return mask


def get_frequency_search_space_indexing(fftlen, sr, masklen, f_tune, f_center_min, f_center_max):
	assert (masklen%2) == 1
	if (f_tune == -1) and (f_center_min == -1) and (f_center_max == -1):
		return np.arange(fftlen -masklen +1, dtype=np.int64) + masklen//2

	freqs = np.fft.fftshift( np.fft.fftfreq(fftlen, d=1.0/sr) ) + f_tune
	df = freqs[1] - freqs[0]
	#assert f_center_min >= freqs[0], (f_center_min, freqs[0])
	#assert f_center_max <= freqs[-1], (f_center_max, freqs[-1])
	assert f_center_max > f_center_min
	indexes = np.zeros(fftlen, dtype=np.int64)
	n_idxs = 0
	for i in range(0, fftlen -masklen +1):
		i_fft = i + masklen//2
		f = freqs[i_fft]
		if (f >= (f_center_min-df)) and (f <= (f_center_max+df)):
			indexes[n_idxs] = i_fft
			n_idxs += 1
	assert n_idxs > 0, (f_center_min, f_center_max, f_center_max - f_center_min, freqs[1]-freqs[0], fftlen - masklen +1, f_center_max > freqs[masklen//2], f_center_min < freqs[fftlen - masklen//2])
	#print("n_idx: ",n_idxs)
	return indexes[:n_idxs]






#@njit(cache=True)
def create_fft_centering_statemx(fftlen, jumplen, sps, baudrate, search_space_triplet, mod_index, BT, c_stat_update, c_f_update_minimum, T_f_upd_recovery, fft_trigger_on_level, fft_trigger_off_level, masklen, avg0, var0, mask_mode, start_margin_mpr, end_margin_mpr):
	assert var0 > 0
	assert fftlen >= 32
	assert mask_mode in (0,1)
	assert (masklen % 2) == 1
	assert T_f_upd_recovery > 0
	f_tune, f_center_min, f_center_max = search_space_triplet
	search_indexes = get_frequency_search_space_indexing(fftlen=fftlen, sr=sps*baudrate, masklen=masklen, f_tune=f_tune, f_center_min=f_center_min, f_center_max=f_center_max)
	n_search = len(search_indexes)

	statemx = np.zeros( (8,fftlen) ,dtype=np.float64 )
	statemx[0,0]  = fftlen
	statemx[0,1]  = jumplen
	statemx[0,2]  = c_stat_update
	statemx[0,3]  = c_f_update_minimum
	statemx[0,4]  = jumplen / (sps*baudrate*T_f_upd_recovery)		# f_updt_recovery_increment
	statemx[0,5]  = fft_trigger_on_level
	statemx[0,6]  = fft_trigger_off_level
	statemx[0,7]  = int(masklen)
	statemx[0,8]  = int(start_margin_mpr * (fftlen+jumplen))
	statemx[0,9]  = int(end_margin_mpr * (fftlen+jumplen))
	statemx[0,10] = n_search
	statemx[0,11] = sps*baudrate

	statemx[0,20] = 0 		# idx  (this runs from (fftlen-jumplen) to fftlen-1 and then an fft is called)
	statemx[0,21] = -1.0	# f_center
	statemx[0,22] = 0		# tx_on
	statemx[0,23] = avg0	# running_avg
	statemx[0,24] = var0	# running_var
	statemx[0,25] = 0		# stat_update_counter
	statemx[0,26] = 0.0		# NEW long running f_center_long
	statemx[0,27] = 1.0		# NEW long running c_f_update_long
	statemx[0,28] = 0.0		# bandmax (just for instrumentation purposes)

	statemx[1,:]  = np.fft.fftshift( np.fft.fftfreq(fftlen, d=1.0) ) # frequency table
	statemx[2,:]  = 0.0		# fft
	statemx[3,:]  = 0.0		# fft mask-correlation
	statemx[4,:]  = 0.0		# window (real) (the only reason this matrix would be complex...)
	statemx[5,:]  = 0.0		# window (imag) (the only reason this matrix would be complex...)
	statemx[7,:n_search] = search_indexes

	if mask_mode == 0:
		statemx[6, 0:masklen]	+= 1.0		# band mask constant term
	else:
		statemx[6, 0:masklen]  	+= construct_fft_mask(sps=sps, mod_index=mod_index, BT=BT, fftlen=fftlen, masklen=masklen, nn=1000) # empiric mask
	return statemx



@njit(cache=True)
def get_center_frequency_estimate(statemx, f_tune):
	c_f_update_long = statemx[0,27]
	is_active = c_f_update_long < 0.999
	f_center_long_normalized = statemx[0,26]  # [0.5 : 0.5)
	sr  = statemx[0,11]
	f_center = f_tune + sr * f_center_long_normalized
	return f_center, is_active


@njit(cache=True)
def compute_fft(x):
	#y = np.zeros_like(x, dtype=np.complex128)
	with objmode(y='complex128[:]'):
		y = np.complex128(np.fft.fft(x))
	return y



@njit(cache=True)
def fft_detect_and_freq_determ(sample_arr, isample0, nsamples, center_f_arr, center_f_head0, statemx, instr_arr):
	fftlen 			= int(statemx[0,0])
	jumplen 		= int(statemx[0,1])
	c_stat_update 	= statemx[0,2]
	c_f_update_minimum 			= statemx[0,3]
	f_updt_recovery_increment 	= statemx[0,4]
	trigger_on_lvl 	= statemx[0,5]
	trigger_off_lvl = statemx[0,6]
	masklen 		= int(statemx[0,7])
	start_margin	= int(statemx[0,8])
	end_margin		= int(statemx[0,9])
	n_search		= int(statemx[0,10])

	idx 			= int(statemx[0,20])
	f_center 		= statemx[0,21]
	tx_on 			= statemx[0,22]
	running_avg 	= statemx[0,23]
	running_var 	= statemx[0,24]
	stat_upd_count 	= statemx[0,25]
	f_center_long   = statemx[0,26]
	c_f_update_long = statemx[0,27]
	bandmax 		= statemx[0,28]
	window 			= statemx[4,:] + 1j*statemx[5,:]
	search_indexes  = statemx[7,:n_search]

	end_tail_remaining = 0
	D_stat_update 	= int(1.0/c_stat_update)
	center_f_head = center_f_head0
	for i_in in range(isample0, isample0 + nsamples):
		if idx >= 0:
			window[idx] = sample_arr[i_in]
		idx += 1
		if idx == fftlen:
			idx = fftlen - jumplen
			#fft = np.abs(np.fft.fftshift(np.fft.fft(window)))
			fft = np.abs(np.fft.fftshift(compute_fft(window)))
			statemx[2,:] = fft
			for i_search in range(n_search):
				i_fft = int(search_indexes[i_search])
				i0 = i_fft - masklen//2
				i1 = i_fft + masklen//2 + 1
				assert i0 >= 0
				assert i0 <= (fftlen-masklen)
				assert i1 >= masklen
				assert i1 <= fftlen
				statemx[3,i_fft] = np.sum(statemx[2,i0:i1] * statemx[6,0:masklen])

			if not tx_on:
				for _ in range(5 + 5*(stat_upd_count < D_stat_update)):
					i_search = np.random.randint(0, n_search)
					i_fft = int(search_indexes[i_search])
					running_avg, running_var = avg_var_upd(avg0=running_avg, var0=running_var, val=statemx[3, i_fft], c_update=c_stat_update, update_count=stat_upd_count)
					stat_upd_count += 1

			argmax 		= np.argmax(statemx[3,:])
			bandmax 	= (statemx[3,argmax] - running_avg) / (running_var**0.5)
			tx_on_prev 	= tx_on
			tx_on 		= (((bandmax > trigger_off_lvl) and tx_on_prev) or (bandmax > trigger_on_lvl)) and (stat_upd_count > D_stat_update)
			if tx_on:
				f_center_long = f_center_long + (statemx[1,:][argmax] - f_center_long) * c_f_update_long
				c_f_update_long = max(c_f_update_minimum,  1/(1 + 1/c_f_update_long))
				f_center 	= f_center_long
			if tx_on and tx_on_prev:
				f_center 	= f_center_long
			if tx_on and (not tx_on_prev):
				f_center 	= f_center_long
				rev_index 	= center_f_head
				while True:
					if (rev_index == 0) or ((center_f_head-rev_index) >= start_margin) or (center_f_arr[rev_index-1] > -0.5): # 'fftlen+jumplen' is the delay of this algorithm.
						break
					rev_index = rev_index -1
				center_f_arr[rev_index:center_f_head] = f_center
			if not tx_on:
				c_f_update_long = min(1.0, c_f_update_long + f_updt_recovery_increment)   # = jumplen / (T_recovery * sr)
				if tx_on_prev:
					end_tail_remaining = end_margin
				if end_tail_remaining <= 0:
					f_center = -1
				end_tail_remaining -= 1

			if jumplen < fftlen:
				window = np.roll(window, -jumplen)
		if center_f_head >= 0:
			center_f_arr[center_f_head] = f_center
			instr_arr[center_f_head,0] = running_avg
			instr_arr[center_f_head,1] = running_var
			instr_arr[center_f_head,2] = bandmax
		center_f_head += 1
	assert center_f_head == center_f_head0 + nsamples
	statemx[0,20] = idx
	statemx[0,21] = f_center
	statemx[0,22] = tx_on
	statemx[0,23] = running_avg
	statemx[0,24] = running_var
	statemx[0,25] = stat_upd_count
	statemx[0,26] = f_center_long
	statemx[0,27] = c_f_update_long
	statemx[0,28] = bandmax
	statemx[4,:] = window.real
	statemx[5,:] = window.imag
	return center_f_head, max(0, center_f_head - (start_margin + 1))   # demodulation head. (the demodulation stage should be given samples up to this head)




@njit(cache=True)
def avg_var_upd(avg0, var0, val, c_update, update_count):
	Dup = 1 + (1 / c_update)
	if update_count < Dup:
		c_update = 1/(update_count*1.0+1)
		pass
	std = var0**0.5

	if (update_count > Dup) and (abs(val-avg0) > (std*4.0)):
		val = avg0 + np.sign(val-avg0)*std*1.4	# gets truncated into 1.4*std, in case we are in the wrong distribution

	avg = avg0 + (val-avg0)*c_update
	varup = (val-avg0)**2
	var = var0 + (varup - var0)*c_update 		# is always positive, as it should be
	return avg, var





