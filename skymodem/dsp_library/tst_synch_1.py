import time
import numpy as np
from kuokka.lib_symsynching import create_classic_JPL_statemx, classic_JPL_synch_run, classic_JPL_synch_step, classic_JPL_synch_strm
from kuokka.lib_symsynching import general_JPL_synch_run_2, create_general_JPL_statemx_2, general_JPL_synch_step_2
from kuokka.lib_symsynching import general_JPL_synch_run_1, general_JPL_synch_step_1, create_general_JPL_statemx_1
from kuokka.lib_symsynching import create_traveling_phase_JPL_statemx, traveling_phase_JPL_synch_strm, traveling_phase_JPL_synch_run
from kuokka.lib_tools import radionoise, make_samples2, make_squarewave
from kuokka.lib_decider import symbol_decision, symbol_decision_f
from scipy.signal import firwin
from matplotlib import pyplot as plt


def _synchs_against_eachother_round(plott=False):
	"""
	This compares the stepped, streamed, and single shot version of classic and general JPL synchronizers against each other,
	and asserts that they produce the same outputs.
	"""
	sps = np.random.randint(11,100) + (np.random.random()-0.5)*0.005
	n_samples = int(sps * 1024)
	stream = np.sin( np.arange(n_samples)*np.pi/sps )
	stream = np.sign(stream) * np.abs(stream)**0.33
	stream = stream + np.random.normal(0, 0.33, n_samples)
	approximate_zeros = np.arange(1024)*sps #*(np.random.random()+0.3)
	if plott:
		fig = plt.figure(figsize=(14,9))
		ax = fig.add_subplot(111)
		ax.plot(np.arange(n_samples), stream)
		ax.grid()
		fig.set_layout_engine("tight")
		plt.show()
	N_eps = int(round(sps))

	mx_clss   = create_classic_JPL_statemx(N_eps=N_eps, n_halflife=12)
	mx_gene_1 = create_general_JPL_statemx_1(sps_int=N_eps, n_decay=12, c_constant=1.0, c_shape=0.0, shape_idx=0)
	mx_gene_2 = create_general_JPL_statemx_2(sps_int=N_eps, n_decay=12, c_constant=1.0, c_shape=0.0, shape_idx=0)

	out_clss = classic_JPL_synch_run(samples=stream, statemx=mx_clss)
	out_gene_1 = general_JPL_synch_run_1(samples=stream, statemx=mx_gene_1)
	out_gene_2 = general_JPL_synch_run_2(samples=stream, statemx=mx_gene_2)
	assert np.allclose(out_clss, out_gene_1)
	assert np.allclose(out_clss, out_gene_2)
	# synch runs produce equal outputs

	out_clss_strm = np.zeros( (len(stream), 3), dtype=np.int64)
	mx_clss   = create_classic_JPL_statemx(N_eps=N_eps, n_halflife=12)
	synch_head = classic_JPL_synch_strm(sample_arr=stream, i_sample0=0, nsamples=len(stream), synch_arr=out_clss_strm, synch_head0=0, statemx=mx_clss)
	assert np.allclose(out_clss_strm, out_clss)
	assert synch_head == len(stream)
	# stream synch (classic) produces the same as single shot run

	mx_clss   = create_classic_JPL_statemx(N_eps=N_eps, n_halflife=12)
	mx_gene_1 = create_general_JPL_statemx_1(sps_int=N_eps, n_decay=12, c_constant=1.0, c_shape=0.0, shape_idx=0)
	mx_gene_2 = create_general_JPL_statemx_2(sps_int=N_eps, n_decay=12, c_constant=1.0, c_shape=0.0, shape_idx=0)
	for i,s in enumerate(stream):
		ring_amax_clss, ring_idx_clss = classic_JPL_synch_step(sample=s, statemx=mx_clss)
		ring_amax_gene_1, ring_idx_gene_1 = general_JPL_synch_step_1(sample=s, statemx=mx_gene_1)
		ring_amax_gene_2, ring_idx_gene_2 = general_JPL_synch_step_2(sample=s, statemx=mx_gene_2)
		assert ring_amax_clss == ring_amax_gene_1
		assert ring_amax_clss == ring_amax_gene_2
		assert ring_idx_clss  == ring_idx_gene_1
		assert ring_idx_clss  == ring_idx_gene_2
		assert out_clss[i][0] == ring_amax_clss
		assert out_clss[i][1] == ring_idx_clss
		# stepping synchronizers produces equal outputs wrt each other, and to the single shot runs above

	dmin_arr = list()
	for isym in range(30, 400):
		approx_zero_i = int(round(approximate_zeros[isym]))
		d1 = (out_clss[approx_zero_i][0] - out_clss[approx_zero_i][1]) % N_eps
		d2 = (out_clss[approx_zero_i][1] - out_clss[approx_zero_i][0]) % N_eps
		dmin = min(d1,d2)
		dmin_arr.append(dmin)
	avg_min_distance = np.average(dmin_arr)
	assert avg_min_distance < 1.1
	# the synchronizer aligns on average ~1 sample from the correct synch. (obs: sps can be up to 100)


def compare_all_JPL_synchronizers_against_eachother():
	"""
	Exectures the _synchs_against_eachother_round() in succession
	"""
	print("="*60)
	print("Testing JPL synchronizer variants produce equal outputs")
	NN = 30
	for ii in range(NN):
		_synchs_against_eachother_round(plott=False)
		if (ii%5) == 0:
			print("{}/{}".format(ii,NN))
	print("{}/{}".format(NN,NN))
	print("="*60)
	print("")





def speedbench_classic_JPL():
	"""
	Measures the speed of classic JPL synch in step- and stream variants.
	"""
	statemx = create_classic_JPL_statemx(N_eps=17, n_halflife=12)

	samples = np.random.normal(0,1, 3000000)
	synch_arr = np.zeros( (len(samples), 3), dtype=np.int64)

	classic_JPL_synch_step(sample=samples[0], statemx=statemx)
	classic_JPL_synch_step(sample=samples[1], statemx=statemx)
	t0 = time.perf_counter()
	for ii, s in enumerate(samples):
		classic_JPL_synch_step(sample=s, statemx=statemx)
	T_call_step = (time.perf_counter() - t0) / len(samples)
	speed_step = 1/T_call_step

	classic_JPL_synch_strm(sample_arr=samples, i_sample0=0, nsamples=512, synch_arr=synch_arr, synch_head0=0, statemx=statemx)
	classic_JPL_synch_strm(sample_arr=samples, i_sample0=0, nsamples=512, synch_arr=synch_arr, synch_head0=0, statemx=statemx)
	t0 = time.perf_counter()
	for _ in range(20):
		classic_JPL_synch_strm(sample_arr=samples, i_sample0=0, nsamples=512, synch_arr=synch_arr, synch_head0=0, statemx=statemx)
	T_call_strm = (time.perf_counter() - t0) / 20
	speed_strm = 512/T_call_strm

	print("="*60)
	print("Timing classic_JPL_synch_step")
	print("speed_step:    {} Ms/s".format( round(1e-6 * speed_step, 4) ))
	print("speed_stream:  {} Ms/s".format( round(1e-6 * speed_strm, 4) ))
	print("="*60)
	print("")




# ============================================================================================================================================================================================
# ============================================================================================================================================================================================
def make_fmdemod_samples(sps_f, baudrate, f_offset_in_br, n_symbols, mod_idx, BT, lp_coeff, noise_W_per_Hz, i_tx_start, nsamples):
	assert n_symbols > (32+6)
	bits = np.random.randint(0,2, n_symbols)*2 - 1
	bits[0:32+6] = np.array( [1,0]*16 + [1,]*6 )*2 - 1
	f_offset = f_offset_in_br * baudrate / (sps_f*baudrate)
	samples, _ = make_samples2(sps_f=sps_f, bitstring=bits, f_offset=f_offset, power=1.0, mod_index=mod_idx, shaper_BT_prod=BT, n_silence_start=i_tx_start, n_silence_end=0)
	samples = np.concatenate( (samples, np.zeros(nsamples-len(samples), dtype=samples.dtype)))
	samples = np.concatenate( (np.zeros(200, dtype=samples.dtype), samples, np.zeros(200, dtype=samples.dtype)))
	samples = samples + radionoise(n=len(samples), sr=sps_f*baudrate, W_per_Hz=noise_W_per_Hz)
	lp_taps = firwin(numtaps=161, cutoff=lp_coeff*baudrate/(sps_f*baudrate), fs=1.0, pass_zero=True)
	lpd = np.convolve(samples, lp_taps)[280:-280]
	dm_zz = lpd * np.conj(np.roll(lpd, 1))
	dmd = np.arctan2(dm_zz.imag, dm_zz.real)
	assert len(dmd) == nsamples, (len(dmd) - nsamples)
	return dmd, bits



def synch_and_decode_experiment(sps, baudrate, n_symbols, relative_rate_error, noisePpHz, f_offset_in_br, mod_idx, BT, lp_coeff, n_decay, synch_delay_mpr, do_prints, do_plots):
	#assert type(sps) == int
	assert type(n_symbols) == int
	assert noisePpHz >= 0.0
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
	n_samples 	= int(T_end * sr)

	# Generating a NRZ sample array from a randomized bitstream, with noise
	t0 = time.perf_counter()
	samples2, bits = make_fmdemod_samples(sps_f=real_sps, baudrate=real_baudrate, f_offset_in_br=f_offset_in_br, n_symbols=n_symbols, mod_idx=mod_idx, BT=BT, lp_coeff=lp_coeff, noise_W_per_Hz=noisePpHz, i_tx_start=i_tx_first, nsamples=n_samples)
	samples0 = make_squarewave(binary_symbols=bits, sps_f=real_sps, i_sample_of_sym0_f=i_tx_start, nsamples=n_samples, npad=0)
	dt_1 = time.perf_counter() - t0

	# Constructing arrays of real symbol centers, and the implied centers if the claimed sps was correct
	t0 = time.perf_counter()
	claimed_centers = list()
	real_centers = list()
	for i_symbol in range(n_symbols):
		c_center = int( round( (T_tx_start + i_symbol/claimed_baudrate)*sr + claimed_sps*0.5 ))
		r_center = (T_tx_start + i_symbol/real_baudrate)*sr + real_sps*0.5
		claimed_centers.append(c_center)
		real_centers.append(r_center)
	dt_2 = time.perf_counter() - t0

	# Do symbol synch
	JPLstatemx = create_classic_JPL_statemx(N_eps=int(claimed_sps), n_halflife=n_decay)
	_ = classic_JPL_synch_run(samples=samples2, statemx=JPLstatemx)
	_ = classic_JPL_synch_run(samples=samples2, statemx=JPLstatemx)
	JPLstatemx = create_classic_JPL_statemx(N_eps=int(claimed_sps), n_halflife=n_decay)
	t00 = time.perf_counter()
	synchphase_arr = classic_JPL_synch_run(samples=samples2, statemx=JPLstatemx)
	dt_3 = time.perf_counter() - t00
	print("Classic synch in     {} ms".format(1000*dt_3))
	print("({} samples)".format(len(samples2)))
	ratio = (sps*9600*1) / (len(samples2) / dt_3)
	print("Core use: ", 100*ratio)
	print("")

	# Symbol decision
	t0 = time.perf_counter()
	assert len(samples2) == len(synchphase_arr)
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
	dt_4 = time.perf_counter() - t0

	# Compute performance metrics
	t0 = time.perf_counter()
	bitcorr = np.correlate(v_msr_arr, bits)
	i_maxcorr = np.argmax(bitcorr)
	corrmax = bitcorr[i_maxcorr]
	sample_timing_errors = i_msr_arr[i_maxcorr:i_maxcorr+n_symbols] - real_centers[0:n_symbols]
	sample_timing_errors = sample_timing_errors / real_sps
	avg_timing_error     = np.average(np.abs(sample_timing_errors))
	dt_5 = time.perf_counter() - t0



	# Traveling synch
	N_eps = int(8)
	synchphase_arr_trv = np.zeros((len(samples2)+10,3), dtype=np.float64)
	JPLstatemx_trv = create_traveling_phase_JPL_statemx(sps_f=sps, N_eps=N_eps, n_decay=n_decay)
	_ = traveling_phase_JPL_synch_strm(sample_arr=samples2, i_sample0=0, nsamples=len(samples2), synch_arr=synchphase_arr_trv, synch_head0=0, statemx=JPLstatemx_trv)
	_ = traveling_phase_JPL_synch_strm(sample_arr=samples2, i_sample0=0, nsamples=len(samples2), synch_arr=synchphase_arr_trv, synch_head0=0, statemx=JPLstatemx_trv)
	#_ = traveling_phase_JPL_synch_run(sample_arr=samples2, statemx=JPLstatemx_trv)
	#_ = traveling_phase_JPL_synch_run(sample_arr=samples2, statemx=JPLstatemx_trv)
	JPLstatemx_trv = create_traveling_phase_JPL_statemx(sps_f=sps, N_eps=N_eps, n_decay=n_decay)
	t00 = time.perf_counter()
	_ = traveling_phase_JPL_synch_strm(sample_arr=samples2, i_sample0=0, nsamples=len(samples2), synch_arr=synchphase_arr_trv, synch_head0=0, statemx=JPLstatemx_trv)
	dt_4 = time.perf_counter() - t00
	print("Traveling synch in  {} ms".format(dt_4*1000))
	print("({} samples)".format(len(samples2)))
	ratio = (sps*9600*1) / (len(samples2) / dt_4)
	print("Core use: ", 100*ratio)
	print("")

	# Symbol decision trv
	t0 = time.perf_counter()
	n_decoded_bits_trv = 0
	optimal_dmd_idx_f_trv = 0.0
	i_msr_arr_trv = list()	# measurement indexes
	v_msr_arr_trv = list()	# measurement values
	while True:
		v_trv, optimal_dmd_idx_f_trv = symbol_decision_f(dmd_arr=samples2, synch_arr=synchphase_arr_trv, dmd_synch_head_i=len(samples2), prev_dmd_idx_f=optimal_dmd_idx_f_trv,
													   sps_f=claimed_sps, synch_delay_i=synch_delay)
		if optimal_dmd_idx_f_trv < 0:
			break
		n_decoded_bits_trv += 1
		i_msr_arr_trv.append(optimal_dmd_idx_f_trv + claimed_sps*0.5)
		v_msr_arr_trv.append( np.sign(v_trv) )
	i_msr_arr_trv = np.array(i_msr_arr_trv)
	v_msr_arr_trv = np.array(v_msr_arr_trv)
	dt_4 = time.perf_counter() - t0

	# Compute performance metrics trv
	t0 = time.perf_counter()
	bitcorr_trv = np.correlate(v_msr_arr_trv, bits)
	i_maxcorr_trv = np.argmax(bitcorr_trv)
	corrmax_trv = bitcorr_trv[i_maxcorr_trv]
	sample_timing_errors_trv = i_msr_arr_trv[i_maxcorr_trv:i_maxcorr_trv+n_symbols] - real_centers[0:n_symbols]
	sample_timing_errors_trv = sample_timing_errors_trv / real_sps
	avg_timing_error_trv     = np.average(np.abs(sample_timing_errors_trv))
	dt_5 = time.perf_counter() - t0




	# Print and plot analytics
	if do_prints:
		report_strings = ["#dt-{}: {} ms".format(i+1, round(1e3*dt, 2)) for i,dt in enumerate( [dt_1,dt_2,dt_3,dt_4,dt_5] )]
		print("\t" + "\n\t".join(report_strings))
		print("{} samples".format( n_samples ))
		print("{} symbols".format( n_symbols ))
		print("corresponds to    {} ms".format( round(1e3 * n_samples / sr) ))
		print("dt-JPL:           {} ms".format( round(1e3 * dt_3, 2) ))
		print("{} decoded bits".format(n_decoded_bits))
		print("Correlation max:  {}".format( corrmax ))
		print("                  {} %".format( round(100*corrmax/n_symbols,1) ))
		print("avg timing error 1: {}".format( round(avg_timing_error,3) ))
		print("avg timing error 2: {}".format( round(avg_timing_error_trv,3) ))

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
		ax2.plot( np.arange(len(synchphase_arr_trv))/real_sps, synchphase_arr_trv[:,0] )
		ax2.grid()

		ax3.plot((0,n_symbols),  (0.5,0.5), color="black", linestyle="--")
		ax3.plot((0,n_symbols),  (-0.5, -0.5), color="black", linestyle="--")
		ax3.plot((0,n_symbols),  (1,1), color="black", linestyle="-")
		ax3.plot((0,n_symbols),  (-1,-1), color="black", linestyle="-")
		ax3.plot(np.arange(n_symbols), sample_timing_errors )
		ax3.plot(np.arange(n_symbols), sample_timing_errors_trv )
		ax3.grid()

		fig.set_layout_engine("tight")
		plt.show()

	return sample_timing_errors, avg_timing_error, corrmax, n_symbols




def simple_synch_and_demod_test(sps, baudrate, relative_rate_error, noisePpHz, do_prints, do_plots):
	ret = synch_and_decode_experiment(sps=sps, baudrate=baudrate, n_symbols=256, relative_rate_error=relative_rate_error, noisePpHz=noisePpHz,
								f_offset_in_br=0.0, mod_idx=0.5, BT=0.5, lp_coeff=0.630, n_decay=30, synch_delay_mpr=12.0,
								do_prints=do_prints, do_plots=do_plots)
	sample_timing_errors, avg_timing_error, corrmax, n_symbols = ret
# ============================================================================================================================================================================================
# ============================================================================================================================================================================================





if __name__ == '__main__':
	#compare_all_JPL_synchronizers_against_eachother()
	#speedbench_classic_JPL()
	synch_and_decode_experiment(sps=12, baudrate=9600, n_symbols=1255, relative_rate_error=1.0e-5, noisePpHz=0.0000023/9600, f_offset_in_br=0.01, mod_idx=0.5, BT=0.5, lp_coeff=0.570,
								n_decay=40, synch_delay_mpr=8, do_plots=True, do_prints=True)













