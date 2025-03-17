import numpy as np
from numba import njit, objmode
from .lib_tools import make_samples


_fft_mask_dict = dict()


def construct_fft_mask(sps, mod_index, BT, fftlen, masklen, nn):
	for key,val in _fft_mask_dict.items():
		k_sps, k_mod_idx, k_BT, k_fftlen, k_masklen, k_nn = key
		if (sps == k_sps) and (mod_index == k_mod_idx) and (BT == k_BT) and (fftlen == k_fftlen) and (masklen == k_masklen) and (nn <= k_nn):
			return val

	assert (masklen % 2) == 1
	nbits = int((fftlen*6 + sps*3 +1) / sps) + 1
	fft_stack = np.zeros(fftlen, dtype=np.float64)
	n_stacked = 0
	while n_stacked < nn:
		bits = np.random.randint(0,2, nbits)*2 - 1
		samples = make_samples(sps_f=sps, bitstring=bits, f_offset=0.0, power=1.0, mod_index=mod_index, shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=8*sps+1)
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
	_fft_mask_dict[ (sps, mod_index, BT, fftlen, masklen, nn) ] = mask
	return mask


def get_masklen(fftlen, sps, mask_mode): #TODO the best centering correlator should really be researched...
	if mask_mode == 0:
		return int(0.5 * fftlen / sps)*2 + 1
	return int(0.5 * 2.5 * fftlen / sps)*2 + 1



def get_frequency_search_space_indexing(fftlen, masklen, f_center_min, f_center_max): #TODO delete?
	assert (masklen%2) == 1
	assert f_center_min <= f_center_max
	assert abs(f_center_min) < 0.5
	assert abs(f_center_max) < 0.5
	freqs = np.fft.fftshift( np.fft.fftfreq(fftlen, d=1.0) )
	df = freqs[1] - freqs[0]
	assert f_center_max > f_center_min
	indexes = np.zeros(fftlen, dtype=np.int64)
	n_idxs = 0
	for i in range(0, fftlen -masklen +1):
		i_fft = i + masklen//2
		f = freqs[i_fft]
		if (f >= (f_center_min-df)) and (f <= (f_center_max+df)):
			indexes[n_idxs] = i_fft
			n_idxs += 1
	assert n_idxs > 0, (f_center_min, f_center_max)
	return indexes[:n_idxs]






#@njit(cache=True)
def create_fft_centering_statemx(fftlen, jumplen, sps, f_center_search_map, mod_index, BT, c_stat_update, n_delay, fft_trigger_on_level, fft_trigger_off_level, avg0, var0, mask_mode, start_margin_mpr, end_margin_mpr):
	assert var0 > 0
	assert fftlen >= 32
	assert mask_mode in (0,1)
	assert int(start_margin_mpr*fftlen) < n_delay
	assert len(f_center_search_map) == fftlen

	masklen = get_masklen(fftlen=fftlen, sps=sps, mask_mode=mask_mode)
	statemx = np.zeros( (8,fftlen) ,dtype=np.float64 )
	statemx[0,0]  = fftlen
	statemx[0,1]  = jumplen
	statemx[0,2]  = c_stat_update
	statemx[0,3]  = n_delay
	statemx[0,4]  = fft_trigger_on_level
	statemx[0,5]  = fft_trigger_off_level
	statemx[0,6]  = int(masklen)
	statemx[0,7]  = int(start_margin_mpr * fftlen)
	statemx[0,8]  = int(end_margin_mpr * fftlen)
	statemx[0,10] = 0.5**(1 / (256*8*sps/jumplen))  # c_f_decay    was[0.5**( 1 / (T_f_decay*sps*baudrate/jumplen))]   [0.5**(1 / ((200*8*sps + n_delay + start_margin_mpr*fftlen)/jumplen))]
	statemx[0,11] = 30*256*8*sps / jumplen	# on_count_limit  was[5.0*sps*baudrate / jumplen]

	statemx[0,20] = 0 		# idx  (this runs from (fftlen-jumplen) to fftlen-1 and then an fft is called)
	statemx[0,21] = -1.0	# f_center
	statemx[0,22] = 0		# trig_on
	statemx[0,23] = avg0	# running_avg
	statemx[0,24] = var0	# running_var
	statemx[0,25] = 0		# stat_update_counter
	statemx[0,26] = 0		# f_switch
	statemx[0,27:29] = 0.0	# f_center_arr
	statemx[0,29] = 0.0		# corrmax (just for instrumentation purposes)
	statemx[0,30] = 0.0		# corrmaxfmax_sum
	statemx[0,31] = 0.0		# corrmax_sum
	statemx[0,32] = 0.0		# on_counter

	statemx[1,:]  = np.fft.fftshift( np.fft.fftfreq(fftlen, d=1.0) ) # frequency table
	statemx[2,:]  = 0.0		# fft
	statemx[3,:]  = 0.0		# fft mask-correlation
	statemx[4,:]  = 0.0		# window (real) (the only reason this matrix would be complex...)
	statemx[5,:]  = 0.0		# window (imag) (the only reason this matrix would be complex...)
	statemx[7,:]  = f_center_search_map

	if mask_mode == 0:
		statemx[6, 0:masklen]	+= 1.0		# band mask constant term
	else:
		statemx[6, 0:masklen]  	+= construct_fft_mask(sps=sps, mod_index=mod_index, BT=BT, fftlen=fftlen, masklen=masklen, nn=1000) # empiric mask
	return statemx



def set_f_center_search_map(statemx, search_map):
	assert len(search_map) == statemx[0,0]
	assert len(search_map) == statemx.shape[1]
	statemx[7,:] = search_map
	statemx[3,:] = 0



@njit(cache=True)
def compute_fft(x):
	#y = np.zeros_like(x, dtype=np.complex128)
	with objmode(y='complex128[:]'):
		y = np.complex128(np.fft.fftshift(np.fft.fft(x)))
	return y



@njit(cache=True)
def fft_detect_and_freq_determ(sample_arr, isample0, nsamples, center_f_arr, center_f_head0, statemx, instr_arr):
	fftlen 			= int(statemx[0,0])
	jumplen 		= int(statemx[0,1])
	c_stat_update 	= statemx[0,2]
	n_delay 		= int(statemx[0,3])
	trigger_on_lvl 	= statemx[0,4]
	trigger_off_lvl = statemx[0,5]
	masklen 		= int(statemx[0,6])
	start_margin	= int(statemx[0,7])
	end_margin		= int(statemx[0,8])
	c_f_decay		= statemx[0,10]
	on_count_limit	= statemx[0,11]

	idx 			= int(statemx[0,20])
	f_center 		= statemx[0,21]
	trig_on 		= statemx[0,22]
	running_avg 	= statemx[0,23]
	running_var 	= statemx[0,24]
	stat_upd_count 	= statemx[0,25]
	f_switch   		= int(statemx[0,26])
	f_center_arr 	= statemx[0,27:29]
	corrmax 		= statemx[0,29]
	corrmaxfmax_sum = statemx[0,30]
	corrmax_sum 	= statemx[0,31]
	on_counter		= statemx[0,32]
	window 			= statemx[4,:] + 1j*statemx[5,:]
	f_center_search_map  	= statemx[7,:]

	D_stat_update 	= int(1.0/c_stat_update)
	center_f_head = center_f_head0
	for i_in in range(isample0, isample0 + nsamples):
		if idx >= 0:
			window[idx] = sample_arr[i_in]
		idx += 1
		if idx == fftlen:
			idx = fftlen - jumplen
			#fft = np.abs(np.fft.fftshift(np.fft.fft(window)))
			fft = np.abs(compute_fft(window))
			statemx[2,:] = fft
			statemx[3,:] = 0
			for i0 in range(fftlen -masklen +1):
				i_center = i0 + masklen//2
				if f_center_search_map[i_center] == 0:
					continue
				i1 = i0 + masklen
				assert i0 >= 0
				assert i0 <= (fftlen-masklen)
				assert i1 >= masklen
				assert i1 <= fftlen
				statemx[3,i_center] = np.sum(statemx[2,i0:i1] * statemx[6,0:masklen])

			argmax 		= np.argmax(statemx[3,:])
			corrmax 	= (statemx[3,argmax] - running_avg) / (running_var**0.5) * (stat_upd_count > D_stat_update)
			trig_on_prev 	= trig_on
			trig_on 		= (((corrmax > trigger_off_lvl) and trig_on_prev) or (corrmax > trigger_on_lvl)) and (stat_upd_count > D_stat_update)

			if not trig_on:
				for _ in range(1 + 1*(stat_upd_count < D_stat_update)):
					i_fft = argmax
					running_avg, running_var = avg_var_upd(avg0=running_avg, var0=running_var, val=statemx[3, i_fft], c_update=c_stat_update, update_count=stat_upd_count)
					stat_upd_count += 1
			if trig_on and (not trig_on_prev):
				#print("    trigger (",corrmax, running_avg, running_var, ")")
				f_switch 			= (1,0)[f_switch]
				center_f_arr[max(0,center_f_head-start_margin):center_f_head] = f_switch-10
			if trig_on:						# fft-mask correlator triggered.
				f_center_arr[f_switch] =  (corrmaxfmax_sum + corrmax*statemx[1,:][argmax]) / (corrmax_sum + corrmax)
				corrmaxfmax_sum	= (corrmaxfmax_sum + corrmax*statemx[1,:][argmax]) * c_f_decay
				corrmax_sum		= (corrmax_sum + corrmax) * c_f_decay
				on_counter 		+= 1
				if on_counter > on_count_limit:
					stat_upd_count = 0
					print("    [DSP]Recalibration limit reached. Statistic counter zeroed.")
			if (not trig_on) and trig_on_prev:
				center_f_arr[center_f_head:center_f_head+end_margin] = f_switch-10
			if not trig_on:
				corrmaxfmax_sum = corrmaxfmax_sum * 0.25
				corrmax_sum		= corrmax_sum * 0.25
				on_counter 		= 0

			if jumplen < fftlen:
				window = np.roll(window, -jumplen)

		if trig_on:
			center_f_arr[center_f_head] = f_switch-10

		if (center_f_head-n_delay) >= 0:
			if center_f_arr[center_f_head-n_delay] >= -0.5:
				center_f_arr[center_f_head-n_delay] = -1.0
			elif center_f_arr[center_f_head-n_delay] < -2.0:
				f_side = int(center_f_arr[center_f_head-n_delay] + 10)
				center_f_arr[center_f_head-n_delay] = f_center_arr[f_side]
		instr_arr[center_f_head,0] = running_avg
		instr_arr[center_f_head,1] = running_var
		instr_arr[center_f_head,2] = corrmax
		center_f_head += 1
	assert center_f_head == center_f_head0 + nsamples
	statemx[0,20] = idx
	statemx[0,21] = f_center
	statemx[0,22] = trig_on
	statemx[0,23] = running_avg
	statemx[0,24] = running_var
	statemx[0,25] = stat_upd_count
	statemx[0,26] = f_switch
	statemx[0,27:29] = f_center_arr
	statemx[0,29] = corrmax
	statemx[0,30] = corrmaxfmax_sum
	statemx[0,31] = corrmax_sum
	statemx[0,32] = on_counter
	statemx[4,:] = window.real
	statemx[5,:] = window.imag
	return center_f_head, max(0, center_f_head - (n_delay + 1))   # demodulation head. (the demodulation stage should be given samples up to this head)



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




