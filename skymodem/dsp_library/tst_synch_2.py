import time
import numpy as np
from matplotlib import pyplot as plt
from kuokka.lib_symsynching import create_general_JPL_statemx_2, general_JPL_synch_run_2, create_classic_JPL_statemx, classic_JPL_synch_run, classic_JPL_synch_strm, classic_JPL_synch_step
from kuokka.lib_decider import symbol_decision
from kuokka.lib_tools import radionoise, make_samples
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



def make_fmdemod_samples(sps_f, baudrate, n_symbols, mod_idx, BT, noise_W_per_Hz, i_tx_start, nsamples):
	assert n_symbols > (32+6)
	bits = np.random.randint(0,2, n_symbols)*2 - 1
	bits[0:32+6] = np.array( [1,0]*16 + [1,]*6 )*2 - 1
	samples = make_samples(sps_f=sps_f, bitstring=bits, f_offset=0.0, power=1.0, mod_index=mod_idx, shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=301, n_silence_start=i_tx_start, n_silence_end=0)
	samples = np.concatenate( (samples, np.zeros(nsamples-len(samples), dtype=samples.dtype)))
	samples = np.concatenate( (np.zeros(200, dtype=samples.dtype), samples, np.zeros(200, dtype=samples.dtype)))
	samples = samples + radionoise(n=len(samples), sr=sps_f*baudrate, W_per_Hz=noise_W_per_Hz)
	lp_taps = firwin(numtaps=201, cutoff=0.623*baudrate/(sps_f*baudrate), fs=1.0, pass_zero=True)
	lpd = np.convolve(samples, lp_taps)[300:-300]
	dm_zz = lpd * np.conj(np.roll(lpd, 1))
	dmd = np.arctan2(dm_zz.imag, dm_zz.real)
	assert len(dmd) == nsamples, (len(dmd) - nsamples)
	return dmd, bits



def tst_synch_and_decode(sps, baudrate, n_symbols, relative_rate_error, noiseamp, synch_mode, n_decay, synch_delay_mpr, do_print, do_plots):
	assert type(sps) == int
	assert type(n_symbols) == int
	assert noiseamp >= 0.0
	assert abs(relative_rate_error) < 0.5
	claimed_sps 				= sps
	sr 							= sps * baudrate
	Ts 							= 1 / sr
	real_sps 					= claimed_sps * (1 + relative_rate_error)
	claimed_baudrate			= baudrate
	real_baudrate				= sr / real_sps
	i_tx_start 					= ((64.0+synch_delay_mpr*2+n_decay*2) * sps) + real_sps * np.random.random()
	T_tx_start 					= i_tx_start * Ts
	td_end_noise				= ((64.0+synch_delay_mpr*2+n_decay*2) * sps) * Ts
	synch_delay 				= int(round(synch_delay_mpr * claimed_sps))
	td_tx 		= n_symbols / real_baudrate
	T_tx_end 	= T_tx_start + td_tx
	T_end 		= T_tx_end + td_end_noise
	i_tx_first 	= int(i_tx_start)
	#i_tx_last 	= int(T_tx_end * sr) + 1
	n_samples 	= int(T_end * sr)
	#n_aftnoise 	= n_samples - i_tx_last

	# Generating a NRZ sample array from a randomized bitstream, with noise
	#bits = np.random.randint(0,2, n_symbols)*2 - 1
	#bits[0:8+7] = np.array( [1,0,1,0,1,0,1,0, 1,1,1,1,1,1,1] )*2 - 1
	#samples0 = make_squarewave(binary_symbols=bits, sps_f=real_sps, i_start_f=i_tx_start, nsamples=n_samples)
	#samples1 = samples0.copy()
	#samples1 						+= np.random.normal(0, noiseamp, n_samples)
	#samples1[:i_tx_first] 			+= np.random.normal(0, noiseamp+1.0, i_tx_first)
	#samples1[i_tx_last:] 			+= np.random.normal(0, noiseamp+1.0, n_aftnoise)
	#samples2 = rollsmooth(samples1, 4)

	samples2, bits = make_fmdemod_samples(sps_f=real_sps, baudrate=real_baudrate, n_symbols=n_symbols, mod_idx=0.5, BT=-1, noise_W_per_Hz=0.22/baudrate, i_tx_start=i_tx_first, nsamples=n_samples)

	# Constructing arrays of real symbol centers, and the implied centers if the claimed sps was correct
	claimed_centers = list()
	real_centers = list()
	for i_symbol in range(n_symbols):
		c_center = int( round( (T_tx_start + i_symbol/claimed_baudrate)*sr + claimed_sps*0.5 ))
		r_center = (T_tx_start + i_symbol/real_baudrate)*sr + real_sps*0.5
		claimed_centers.append(c_center)
		real_centers.append(r_center)

	# Do symbol synch
	if synch_mode == 0:
		JPLstatemx = create_classic_JPL_statemx(N_eps=claimed_sps, n_decay=n_decay)
		_ = classic_JPL_synch_run(samples=samples2, statemx=JPLstatemx)
		_ = classic_JPL_synch_run(samples=samples2, statemx=JPLstatemx)
		t0 = time.perf_counter()
		synchphase_arr = classic_JPL_synch_run(samples=samples2, statemx=JPLstatemx)
		dt1 = time.perf_counter() - t0
	else:
		JPLstatemx = create_general_JPL_statemx_2(sps_int=claimed_sps, n_decay=n_decay, c_constant=1.0, c_shape=0.0, shape_idx=0)
		_ = general_JPL_synch_run_2(samples=samples2, statemx=JPLstatemx.copy())
		_ = general_JPL_synch_run_2(samples=samples2, statemx=JPLstatemx.copy())
		t0 = time.perf_counter()
		synchphase_arr = general_JPL_synch_run_2(samples=samples2, statemx=JPLstatemx)
		dt1 = time.perf_counter() - t0

	# Symbol decision
	assert len(samples2) == len(synchphase_arr)
	_ = symbol_decision(dmd_arr=samples2, synch_arr=synchphase_arr, dmd_synch_head_i=len(samples2), prev_dmd_idx_f=0.0, sps_f=claimed_sps, synch_delay_i=synch_delay, N_eps_i=int(claimed_sps))
	_ = symbol_decision(dmd_arr=samples2, synch_arr=synchphase_arr, dmd_synch_head_i=len(samples2), prev_dmd_idx_f=0.0, sps_f=claimed_sps, synch_delay_i=synch_delay, N_eps_i=int(claimed_sps))
	n_decoded_bits = 0
	optimal_dmd_idx_f = 0.0
	i_msr_arr = list()	# measurement indexes
	v_msr_arr = list()	# measurement values
	while True:
		v, optimal_dmd_idx_f = symbol_decision(dmd_arr=samples2, synch_arr=synchphase_arr, dmd_synch_head_i=len(samples2), prev_dmd_idx_f=optimal_dmd_idx_f, sps_f=claimed_sps, synch_delay_i=synch_delay, N_eps_i=int(claimed_sps))
		if optimal_dmd_idx_f < 0:
			break
		n_decoded_bits += 1
		i_msr_arr.append(optimal_dmd_idx_f + claimed_sps*0.5)
		v_msr_arr.append( np.sign(v) )
	i_msr_arr = np.array(i_msr_arr)
	v_msr_arr = np.array(v_msr_arr)
	bitcorr = np.correlate(v_msr_arr, bits)
	i_maxcorr = np.argmax(bitcorr)
	corrmax = bitcorr[i_maxcorr]

	sample_timing_errors = i_msr_arr[i_maxcorr:i_maxcorr+n_symbols] - real_centers[0:n_symbols]
	sample_timing_errors = sample_timing_errors / real_sps
	avg_timing_error     = np.average(np.abs(sample_timing_errors))

	if do_print:
		print("{} samples".format( n_samples ))
		print("{} symbols".format( n_symbols ))
		print("corresponds to    {} ms".format( round(1e3 * n_samples / sr) ))
		print("dt-JPL:           {} ms".format( round(1e3 * dt1, 2) ))
		print("{} decoded bits".format(n_decoded_bits))
		print("Correlation max:  {}".format( corrmax ))
		print("                  {} %".format( round(100*corrmax/n_symbols,1) ))
		print("avg timing error: {}".format( round(avg_timing_error,3) ))

	if do_plots:
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

		ax3.plot((0,n_symbols),  (0.5,0.5), color="black", linestyle="--")
		ax3.plot((0,n_symbols),  (-0.5, -0.5), color="black", linestyle="--")
		ax3.plot((0,n_symbols),  (1,1), color="black", linestyle="-")
		ax3.plot((0,n_symbols),  (-1,-1), color="black", linestyle="-")
		ax3.plot(np.arange(n_symbols), sample_timing_errors )
		ax3.grid()

		fig.set_layout_engine("tight")
		plt.show()

	return sample_timing_errors, avg_timing_error, corrmax, n_symbols



#def run_comparison_test():



# (0.5* 0.2*sps )**0.5
# Pd = 0.2 / (sr/sps)
# Pd = 0.2*sps / sr

# sps = sr/baudrate



tst_synch_and_decode(sps=21, baudrate=9600, n_symbols=2*1024, relative_rate_error=2.5e-5, noiseamp=1.2, synch_mode=0, n_decay=24, synch_delay_mpr=8.0, do_print=True, do_plots=True)

t00 = time.perf_counter()
for _ in range(4):
	tst_synch_and_decode(sps=21, baudrate=9600, n_symbols=2*1024, relative_rate_error=2.5e-5, noiseamp=1.2, synch_mode=0, n_decay=12, synch_delay_mpr=6.0, do_print=False, do_plots=False)
dt_all = (time.perf_counter() - t00) / 4
print("One run: {} ms".format(  round(1e3*dt_all, 3) ))
print("        ~{} /s".format(  round(1/dt_all, 1) ))





























