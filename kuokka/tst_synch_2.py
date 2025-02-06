import time
import numpy as np
from matplotlib import pyplot as plt
from kuokka.lib_symsynching import create_general_JPL_statemx_2, general_JPL_synch_run_2
from kuokka.lib_decider import symbol_decision
from scipy.signal import firwin
#def f_tria(x,m):
#	x2 = x % m
#	x2 = x2 + (m-x2*2)*(x2>(m/2))
#	return x2



def rollsmooth(arr, n):
	arr_ = arr * 1.0
	for _ in range(n):
		arr_ = (arr_ + np.roll(arr_, 1) + np.roll(arr_, -1)) * (1/3)
	return arr_




def make_squarewave(binary_symbols, sps_f, i_start_f, nsamples=-1):
	assert np.all(np.isclose(binary_symbols, 1) + np.isclose(binary_symbols, -1))
	if nsamples < 0:
		nsamples = int(len(binary_symbols) * sps_f + i_start_f)
	samples = np.zeros(nsamples)
	for i in range(nsamples):
		isym = int((i-i_start_f) / sps_f)
		if 0 <= isym < len(binary_symbols):
			samples[i] = binary_symbols[isym]
	return samples




def tst_synch_and_decode():
	claimed_sps 				= 21
	sr 							= claimed_sps * 9600.0
	Ts 							= 1 / sr
	real_sps 					= claimed_sps * (1 - 12e-4)
	n_symbols 					= 8*256
	claimed_baudrate			= sr / claimed_sps
	real_baudrate				= sr / real_sps
	i_tx_start 					= 1200.0 + real_sps * np.random.random()
	T_tx_start 					= i_tx_start * Ts
	td_end_noise				= (1000.0 + 0.0) * Ts
	sigamp 						= 1.0
	noiseamp 					= 1.0
	n_decay 					= 12
	synch_delay 				= 6 * int(round(claimed_sps))
	td_tx = n_symbols / real_baudrate
	T_tx_end = T_tx_start + td_tx
	T_end = T_tx_end + td_end_noise
	i_tx_first = int(i_tx_start)
	i_tx_last = int(T_tx_end * sr) + 1
	n_samples = int(T_end * sr)
	n_aftnoise = n_samples - i_tx_last
	print("{} samples".format( n_samples ))
	print("{} symbols".format( n_symbols ))
	print("corresponds to {} ms".format( round(1e3 * n_samples / sr) ))

	bits = np.random.randint(0,2, n_symbols)*2 - 1
	bits[0:8+7] = np.array( [1,0,1,0,1,0,1,0, 1,1,1,1,1,1,1] )*2 - 1
	samples0 = make_squarewave(binary_symbols=bits, sps_f=real_sps, i_start_f=i_tx_start, nsamples=n_samples)
	#lp_taps = firwin(numtaps=201, cutoff=real_baudrate*2*0.8/(real_baudrate*real_sps), pass_zero=True)
	#samples0 = np.convolve(samples0, lp_taps)[100:-100]
	samples1 = samples0.copy()
	samples1 						+= np.random.normal(0, noiseamp, n_samples)
	samples1[:i_tx_first] 			+= np.random.normal(0, noiseamp+sigamp, i_tx_first)
	samples1[i_tx_last:] 			+= np.random.normal(0, noiseamp+sigamp, n_aftnoise)
	samples2 = rollsmooth(samples1, 4)

	claimed_centers = list()
	real_centers = list()
	for i_symbol in range(n_symbols):
		c_center = int( round( (T_tx_start + i_symbol/claimed_baudrate)*sr + claimed_sps*0.5 ))
		r_center = (T_tx_start + i_symbol/real_baudrate)*sr + real_sps*0.5
		claimed_centers.append(c_center)
		real_centers.append(r_center)


	JPLstatemx = create_general_JPL_statemx_2(sps_int=claimed_sps, n_decay=n_decay, c_constant=1.0, c_shape=0.0, shape_idx=0)
	_ = general_JPL_synch_run_2(samples=samples2, statemx=JPLstatemx.copy())
	_ = general_JPL_synch_run_2(samples=samples2, statemx=JPLstatemx.copy())
	t0 = time.perf_counter()
	synchphase_arr = general_JPL_synch_run_2(samples=samples2, statemx=JPLstatemx)
	dt1 = time.perf_counter() - t0
	print("dt-gJPL:   {} ms".format( round(1e3 * dt1, 2) ))

	assert len(samples2) == len(synchphase_arr)
	n_demodulated = len(samples2)
	print("::", n_demodulated, len(samples2))
	_ = symbol_decision(dmd_arr=samples2, synch_arr=synchphase_arr, dmd_synch_head_i=len(samples2), prev_dmd_idx_f=0.0, sps_f=claimed_sps, synch_delay_i=synch_delay, N_eps_i=int(claimed_sps))
	_ = symbol_decision(dmd_arr=samples2, synch_arr=synchphase_arr, dmd_synch_head_i=len(samples2), prev_dmd_idx_f=0.0, sps_f=claimed_sps, synch_delay_i=synch_delay, N_eps_i=int(claimed_sps))
	n_decoded_bits = 0
	optimal_dmd_idx_f = 0.0
	i_msr_arr = list()
	v_msr_arr = list()
	dt2 = 0
	while True:
		t0 = time.perf_counter()
		v, optimal_dmd_idx_f = symbol_decision(dmd_arr=samples2, synch_arr=synchphase_arr, dmd_synch_head_i=len(samples2), prev_dmd_idx_f=optimal_dmd_idx_f, sps_f=claimed_sps, synch_delay_i=synch_delay, N_eps_i=int(claimed_sps))
		dt2 += time.perf_counter() - t0
		if optimal_dmd_idx_f < 0:
			break
		n_decoded_bits += 1
		i_msr_arr.append(optimal_dmd_idx_f + claimed_sps*0.5)
		v_msr_arr.append( np.sign(v) )
	print("dt-decide: {} ms".format( round(1e3 * dt2, 2) ))
	print("{} decoded bits".format(n_decoded_bits))
	i_msr_arr = np.array(i_msr_arr)
	v_msr_arr = np.array(v_msr_arr)
	bitcorr = np.correlate(v_msr_arr, bits)
	i_maxcorr = np.argmax(bitcorr)
	corrmax = bitcorr[i_maxcorr]
	print("Correlation max: {}".format( corrmax ))
	print("                 {} %".format( round(100*corrmax/n_symbols,1) ))



	n_timing_sample = 2048
	sample_timing_errors = i_msr_arr[i_maxcorr:i_maxcorr+n_timing_sample] - real_centers[0:n_timing_sample]

	fig= plt.figure(figsize=(20,14))
	ax1 = fig.add_subplot(221)
	ax2 = fig.add_subplot(222)
	ax3 = fig.add_subplot(212)

	ax1.plot( np.arange(n_samples), samples0 )
	ax1.plot( np.arange(n_samples), samples2 )
	ax1.scatter( real_centers, bits*1.15, color="black")
	ax1.scatter( i_msr_arr, v_msr_arr*1.2, color="green", marker="o")
	ax1.scatter( i_msr_arr[i_maxcorr:i_maxcorr+8], v_msr_arr[i_maxcorr:i_maxcorr+8]*1.25, color="green", marker="x")
	ax1.scatter( claimed_centers, bits*1.3, color="red", marker="x")
	ax1.grid()

	ax2.plot( np.arange(len(synchphase_arr))/real_sps, synchphase_arr[:,0] )
	ax2.grid()

	ax3.plot((0,n_timing_sample),  (real_sps/2,)*2, color="black", linestyle="--")
	ax3.plot((0,n_timing_sample),  (-real_sps/2,)*2, color="black", linestyle="--")
	ax3.plot((0,n_timing_sample),  (real_sps,)*2, color="black", linestyle="-")
	ax3.plot((0,n_timing_sample),  (-real_sps,)*2, color="black", linestyle="-")
	ax3.plot(np.arange(n_timing_sample), sample_timing_errors )
	ax3.grid()

	fig.set_layout_engine("tight")
	plt.show()













tst_synch_and_decode()






























