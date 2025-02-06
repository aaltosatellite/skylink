import numpy as np
from numba import njit, int64


# === CLASSIC ===============================================================================================================================================================
# ===========================================================================================================================================================================
@njit(cache=True)
def create_classic_JPL_statemx(N_eps, n_decay):
	assert n_decay >= 1.0
	statemx = np.zeros((3, N_eps), dtype=np.float64)
	statemx[0,0] = 1 - 1/n_decay
	statemx[0,1] = 0	# absolute index
	statemx[1,:] *= 0.0 # sliding window
	statemx[2,:] *= 0.0 # ring accumulator
	return statemx



@njit(cache=True)
def classic_JPL_synch_reset(statemx):
	statemx[0,1] = 0  				# absolute index
	statemx[1] = statemx[1] * 0.0  	# sliding window
	statemx[2] = statemx[2] * 0.0	# ring accumulator



@njit(cache=True)
def classic_JPL_synch_run(samples, statemx):
	"""
	This is a simple implementation of a maximum likelihood synchronizer for NRZ data stream described in
	p.340 of the 9th volume of the DEEP SPACE COMMUNICATIONS AND NAVIGATION SERIES (DESCANSO) published by JPL.

	It finds a maximum amplitude producing phase of the data stream, given that symbol length matches closely to the
	length of the sliding_window and ring_accumulator. This match has to be ~1% or better.

	This is a special case of a more generic synchronizer, and hence possibly a suboptimal one.
	The general JPL synchronizer with parameters c_constant=1, c_shape=0 and normalized_product=False is equivalent to this.

	:param samples: 			1D array of demodulated real samples (float64 or float32)
	:param statemx: 			2D state matrix of the synchronizer.
	:return:
	"""
	assert statemx.shape[0] == 3
	N_eps = statemx.shape[1]
	synchphase_arr = np.zeros( (len(samples),3), dtype=np.int64)   # should this be specified as int64 ? (all components are integers...)
	c_decay = statemx[0,0]
	absolute_idx = int(statemx[0,1])
	i_out = 0
	for s in samples:
		ring_idx = absolute_idx % N_eps
		statemx[1][ring_idx] = s
		statemx[2][ring_idx] += np.abs(np.sum(statemx[1]))   # np.abs(np.sum(ring))   VS  np.log(np.cosh(np.sum(ring*cc)))   # cc ~ 1/( avg_sample_amplitude*N_eps)
		statemx[2][ring_idx] *= c_decay
		synchphase_arr[i_out][0] = np.argmax(statemx[2])
		synchphase_arr[i_out][1] = ring_idx
		synchphase_arr[i_out][2] = absolute_idx
		i_out += 1
		absolute_idx += 1
	statemx[0,1] = float(absolute_idx)
	return synchphase_arr



@njit(cache=True)
def classic_JPL_synch_strm(sample_arr, i_sample0, nsamples, synch_arr, synch_head0, statemx):
	assert type(synch_arr[0,0]) is int64
	assert synch_arr.shape[1] == 3
	N_eps = statemx.shape[1]
	c_decay = statemx[0,0]
	absolute_idx = int(statemx[0,1])
	synch_head = synch_head0
	for i_sample in range(i_sample0, i_sample0 + nsamples):
		ring_idx = absolute_idx % N_eps
		statemx[1][ring_idx] = sample_arr[i_sample]
		statemx[2][ring_idx] += np.abs(np.sum(statemx[1]))   # np.abs(np.sum(ring))   VS  np.log(np.cosh(np.sum(ring*cc)))   # cc ~ 1/( avg_sample_amplitude*N_eps)
		statemx[2][ring_idx] *= c_decay
		synch_arr[synch_head][0] = np.argmax(statemx[2])
		synch_arr[synch_head][1] = ring_idx
		synch_arr[synch_head][2] = absolute_idx
		synch_head += 1
		absolute_idx += 1
	statemx[0,1] = float(absolute_idx)
	return synch_head



@njit(cache=True)
def classic_JPL_synch_step(sample, statemx):
	ring_idx = int(statemx[0,1]) % statemx.shape[1]
	statemx[0,1] += 1
	#ring_idx = sample_abs_idx % statemx.shape[1]
	statemx[1][ring_idx] = sample
	statemx[2][ring_idx] += np.abs(np.sum(statemx[1]))
	statemx[2][ring_idx] *= statemx[0,0] # c_decay
	return np.argmax(statemx[2]), ring_idx  #, sample_abs_idx
# === CLASSIC ===============================================================================================================================================================
# ===========================================================================================================================================================================







# === GENERAL ===============================================================================================================================================================
# ===========================================================================================================================================================================
@njit(cache=True)
def create_general_JPL_statemx_1(sps_int, n_decay, c_constant, c_shape, shape_idx):
	assert sps_int > 0
	assert shape_idx in (0, 1)
	if shape_idx == 0:
		shape = np.cos(np.linspace(-np.pi / 2, np.pi / 2, sps_int))
	else:
		shape = np.sinc(np.linspace(-1, 1, sps_int))
	shape  = shape - np.average(shape)
	shape  = shape / np.average(np.abs(shape))
	mask   = shape*c_shape + c_constant
	statemx = np.zeros((4, sps_int), dtype=np.float64)
	statemx[0,0] = 1 - 1/n_decay
	statemx[0,1] = 0	# absolute index
	statemx[1] *= 0.0  	# sliding window
	statemx[2] *= 0.0  	# ring accumulator
	statemx[3] = mask
	return statemx



@njit(cache=True)
def create_general_JPL_statemx_2(sps_int, n_decay, c_constant, c_shape, shape_idx):
	assert sps_int > 0
	assert shape_idx in (0, 1)
	if shape_idx == 0:
		shape = np.cos(np.linspace(-np.pi / 2, np.pi / 2, sps_int))
	else:
		shape = np.sinc(np.linspace(-1, 1, sps_int))
	shape  = shape - np.average(shape)
	shape  = shape / np.average(np.abs(shape))
	mask   = shape*c_shape + c_constant
	statemx = np.zeros((3+sps_int, sps_int), dtype=np.float64)
	statemx[0,0] = 1 - 1/n_decay
	statemx[0,1] = 0	# absolute index
	statemx[1] *= 0.0  	# sliding window
	statemx[2] *= 0.0  	# ring accumulator
	for i in range(sps_int):
		statemx[3+i] = np.roll(mask, i+1)		# mask in different rolled phases
	return statemx



@njit(cache=True)
def general_JPL_synch_reset(statemx):
	statemx[0,1] = 0.0  			# absolute index
	statemx[1] = statemx[1] * 0.0  	# sliding window
	statemx[2] = statemx[2] * 0.0	# ring accumulator



@njit(cache=True)
def general_JPL_synch_run_1(samples, statemx):
	N_eps = statemx.shape[1]
	synchphase_arr = np.zeros( (len(samples),3), dtype=np.int64 )
	c_decay = statemx[0,0]
	i_out = 0
	absolute_idx = int(statemx[0,1])
	for s in samples:
		ring_idx = absolute_idx % N_eps
		statemx[1] = np.roll(statemx[1], -1)
		statemx[1][-1] = s
		waveprod = statemx[1] * statemx[3]
		statemx[2][ring_idx] += np.abs(np.sum(waveprod))   # np.abs(np.sum(waveprod))   VS  np.log(np.cosh(np.sum(waveprod * cc)))   # cc ~ 1/( avg_sample_amplitude*N_eps)
		statemx[2][ring_idx] *= c_decay
		synchphase_arr[i_out][0] = np.argmax(statemx[2])
		synchphase_arr[i_out][1] = ring_idx
		synchphase_arr[i_out][2] = absolute_idx
		i_out += 1
		absolute_idx += 1
	statemx[0,1] = absolute_idx
	return synchphase_arr



@njit(cache=True)
def general_JPL_synch_step_1(sample, statemx):
	ring_idx = int(statemx[0,1]) % statemx.shape[1]
	statemx[0,1] += 1
	statemx[1] = np.roll(statemx[1], -1)
	statemx[1][-1] = sample
	waveprod = statemx[1] * statemx[3]
	statemx[2][ring_idx] += np.abs(np.sum(waveprod))
	statemx[2][ring_idx] *= statemx[0,0] # c_decay
	return np.argmax(statemx[2]), ring_idx  #, sample_abs_idx



@njit(cache=True)
def general_JPL_synch_run_2(samples, statemx):
	# optimization: Window never rolls, instead statemx contains N_eps rolled versions of the mask.
	N_eps = statemx.shape[1]
	synchphase_arr = np.zeros( (len(samples),3), dtype=np.int64 )
	c_decay = statemx[0,0]
	i_out = 0
	absolute_idx = int(statemx[0,1])
	for s in samples:
		ring_idx = absolute_idx % N_eps
		statemx[1][ring_idx] = s
		waveprod = statemx[1] * statemx[3 + ring_idx]
		statemx[2][ring_idx] += np.abs(np.sum(waveprod))   # np.abs(np.sum(waveprod))   VS  np.log(np.cosh(np.sum(waveprod * cc)))   # cc ~ 1/( avg_sample_amplitude*N_eps)
		statemx[2][ring_idx] *= c_decay
		synchphase_arr[i_out][0] = np.argmax(statemx[2])
		synchphase_arr[i_out][1] = ring_idx
		synchphase_arr[i_out][2] = absolute_idx
		i_out += 1
		absolute_idx += 1
	statemx[0,1] = absolute_idx
	return synchphase_arr



@njit(cache=True)
def general_JPL_synch_step_2(sample, statemx):
	ring_idx = int(statemx[0,1]) % statemx.shape[1]
	statemx[0,1] += 1
	statemx[1][ring_idx] = sample
	waveprod = statemx[1] * statemx[3 + ring_idx]
	statemx[2][ring_idx] += np.abs(np.sum(waveprod))
	statemx[2][ring_idx] *= statemx[0,0] # c_decay
	return np.argmax(statemx[2]), ring_idx  #, sample_abs_idx
# === GENERAL ===============================================================================================================================================================
# ===========================================================================================================================================================================






