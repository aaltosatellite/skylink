import numpy as np
from numba import njit, int64, float64



@njit(cache=True)
def can_decide(prev_dmd_idx_f, dmd_synch_head_i, sps, synch_delay):
	if dmd_synch_head_i < (prev_dmd_idx_f + sps * 2):
		return False
	if dmd_synch_head_i < (prev_dmd_idx_f + synch_delay + 2):
		return False
	return True



@njit(cache=True)
def symbol_decision(dmd_arr, synch_arr, dmd_synch_head_i, prev_dmd_idx_f, sps_f, synch_delay_i, N_eps_i):
	assert type(synch_arr[0,0]) is int64
	if not can_decide(prev_dmd_idx_f=prev_dmd_idx_f, dmd_synch_head_i=dmd_synch_head_i, sps=sps_f, synch_delay=synch_delay_i):
		return 0, -1
	synch_tap_idx = int(round(prev_dmd_idx_f + synch_delay_i))
	assert synch_tap_idx >= 0
	ring_amax_at_tap, rel_idx_at_tap, _ = synch_arr[synch_tap_idx]
	an_index_in_synch = synch_tap_idx + ((ring_amax_at_tap - rel_idx_at_tap) % N_eps_i)

	# method 2
	f1 = prev_dmd_idx_f + sps_f
	f2_A = f1 + ((an_index_in_synch-f1) % N_eps_i) + 0.5
	f2_B = f2_A - N_eps_i
	f2 = f2_A+0.0 if (abs(f2_A - f1) < abs(f2_B - f1)) else f2_B+0.0
	optimal_dmd_idx_f = f1*0.75 + f2*0.25

	# method 2 safe
	#f1_ = prev_dmd_idx_f + sps_f
	#f2_A_ = f1_ + ((synch_abs_idx_i-f1_) % N_eps_i) + 0.5
	#f2_B_ = f2_A_ - N_eps_i
	#f2_ = f2_A_ if (abs(f2_A_ - f1_) < abs(f2_B_ - f1_)) else f2_B_
	#assert np.isclose(optimal_dmd_idx_f, f1_*0.5 + f2_*0.5)

	idx_actual = int(round(optimal_dmd_idx_f))
	v = np.sum(dmd_arr[idx_actual:idx_actual+int(sps_f)])
	return v, optimal_dmd_idx_f


@njit(cache=True)
def symbol_decision_f(dmd_arr, synch_arr, dmd_synch_head_i, prev_dmd_idx_f, sps_f, synch_delay_i):
	assert type(synch_arr[0,0]) is float64
	if not can_decide(prev_dmd_idx_f=prev_dmd_idx_f, dmd_synch_head_i=dmd_synch_head_i, sps=sps_f, synch_delay=synch_delay_i):
		return 0, -1
	synch_tap_idx = int(round(prev_dmd_idx_f + synch_delay_i))
	assert synch_tap_idx >= 0
	max_phase_at_tap, phase0_at_tap, _ = synch_arr[synch_tap_idx]
	an_index_in_synch = synch_tap_idx + 0.5 + ((max_phase_at_tap - phase0_at_tap + 0.0) * sps_f)


	# method 2
	f1 = prev_dmd_idx_f + sps_f
	f2_A = f1 + ((an_index_in_synch-f1) % sps_f) + 0.0
	f2_B = f2_A - sps_f
	f2 = f2_A if (abs(f2_A - f1) < abs(f2_B - f1)) else f2_B
	optimal_dmd_idx_f = f1*0.75 + f2*0.25

	i1 = int(round(optimal_dmd_idx_f))
	i2 = int(round(optimal_dmd_idx_f+sps_f))
	v = np.sum(dmd_arr[i1:i2])
	return v, optimal_dmd_idx_f
















