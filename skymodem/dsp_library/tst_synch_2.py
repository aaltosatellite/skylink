import pickle
import time
import numpy as np
from matplotlib import pyplot as plt
from kuokka.lib_symsynching import create_general_JPL_statemx_2, general_JPL_synch_run_2, create_classic_JPL_statemx, classic_JPL_synch_run, classic_JPL_synch_strm, classic_JPL_synch_step
from kuokka.lib_decider import symbol_decision
from kuokka.lib_tools import radionoise, make_samples, make_squarewave
from scipy.signal import firwin
from mtools.tools_system import mpr_set
from numba import njit
from datetime import datetime as dtime

#def f_tria(x,m):
#	x2 = x % m
#	x2 = x2 + (m-x2*2)*(x2>(m/2))
#	return x2


def make_fmdemod_samples(sps_f, baudrate, f_offset_in_br, n_symbols, mod_idx, BT, lp_coeff, noise_W_per_Hz, i_tx_start, nsamples):
	assert n_symbols > (32+6)
	bits = np.random.randint(0,2, n_symbols)*2 - 1
	bits[0:32+6] = np.array( [1,0]*16 + [1,]*6 )*2 - 1
	f_offset = f_offset_in_br * baudrate / (sps_f*baudrate)
	samples = make_samples(sps_f=sps_f, bitstring=bits, f_offset=f_offset, power=1.0, mod_index=mod_idx, shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=int(sps_f)*6+1, n_silence_start=i_tx_start, n_silence_end=0)
	samples = np.concatenate( (samples, np.zeros(nsamples-len(samples), dtype=samples.dtype)))
	samples = np.concatenate( (np.zeros(200, dtype=samples.dtype), samples, np.zeros(200, dtype=samples.dtype)))
	samples = samples + radionoise(n=len(samples), sr=sps_f*baudrate, W_per_Hz=noise_W_per_Hz)
	lp_taps = firwin(numtaps=161, cutoff=lp_coeff*baudrate/(sps_f*baudrate), fs=1.0, pass_zero=True)
	lpd = np.convolve(samples, lp_taps)[280:-280]
	dm_zz = lpd * np.conj(np.roll(lpd, 1))
	dmd = np.arctan2(dm_zz.imag, dm_zz.real)
	assert len(dmd) == nsamples, (len(dmd) - nsamples)
	return dmd, bits



def tst_synch_and_decode(sps, baudrate, n_symbols, relative_rate_error, noisePpHz, f_offset_in_br, synch_mode, mod_idx, BT, lp_coeff, n_decay, synch_delay_mpr, do_prints, do_plots):
	assert type(sps) == int
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
	t0 = time.perf_counter()
	if synch_mode == 0:
		JPLstatemx = create_classic_JPL_statemx(N_eps=claimed_sps, n_decay=n_decay)
		#_ = classic_JPL_synch_run(samples=samples2, statemx=JPLstatemx)
		#_ = classic_JPL_synch_run(samples=samples2, statemx=JPLstatemx)
		t00 = time.perf_counter()
		synchphase_arr = classic_JPL_synch_run(samples=samples2, statemx=JPLstatemx)
		dt_3_1 = time.perf_counter() - t00
	else:
		JPLstatemx = create_general_JPL_statemx_2(sps_int=claimed_sps, n_decay=n_decay, c_constant=1.0, c_shape=0.0, shape_idx=0)
		_ = general_JPL_synch_run_2(samples=samples2, statemx=JPLstatemx.copy())
		_ = general_JPL_synch_run_2(samples=samples2, statemx=JPLstatemx.copy())
		t00 = time.perf_counter()
		synchphase_arr = general_JPL_synch_run_2(samples=samples2, statemx=JPLstatemx)
		dt_3_1 = time.perf_counter() - t00
	dt_3 = time.perf_counter() - t0

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

	# Print and plot analytics
	if do_prints:
		report_strings = ["#dt-{}: {} ms".format(i+1, round(1e3*dt, 2)) for i,dt in enumerate( [dt_1,dt_2,dt_3,dt_4,dt_5] )]
		print("\t" + "\n\t".join(report_strings))
		print("{} samples".format( n_samples ))
		print("{} symbols".format( n_symbols ))
		print("corresponds to    {} ms".format( round(1e3 * n_samples / sr) ))
		print("dt-JPL:           {} ms".format( round(1e3 * dt_3_1, 2) ))
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


def surf_integral(x_arr, y_arr):
	A = 0
	assert len(x_arr) > 1
	assert len(x_arr) == len(y_arr)
	for i in range(len(x_arr)-1):
		assert x_arr[i+1] > x_arr[i]
		dx = x_arr[i+1] - x_arr[i]
		A += dx * (y_arr[i] + y_arr[i+1])*0.5
	return A



def evaluate_param_kwargs(kwargs, noise_p_array, n_runs_per, do_plots=False):
	avg_decode_rate_arr = np.zeros(len(noise_p_array))*0.0
	avg_avg_timing_error_arr = np.zeros(len(noise_p_array))*0.0
	for i_noise, noise_p in enumerate(noise_p_array):
		avg_decode_rate = 0.0
		avg_avg_timing_error = 0.0
		_kwgs = kwargs.copy()
		_kwgs["noisePpHz"] = noise_p
		for i_run in range(n_runs_per):
			sample_timing_errors, avg_timing_error, corrmax, n_symbols = tst_synch_and_decode(**_kwgs)
			avg_decode_rate += (corrmax+(n_symbols-corrmax)*0.5) / n_symbols
			avg_avg_timing_error += avg_timing_error
		avg_decode_rate_arr[i_noise] = avg_decode_rate / n_runs_per
		avg_avg_timing_error_arr[i_noise] = avg_avg_timing_error / n_runs_per
	criterion_arr = avg_decode_rate_arr**2
	A = surf_integral(x_arr=noise_p_array, y_arr=criterion_arr)
	if do_plots:
		fig= plt.figure(figsize=(12,12))
		ax1 = fig.add_subplot(111)
		ax1.plot(noise_p_array, avg_decode_rate_arr)
		ax1.plot(noise_p_array, criterion_arr)
		ax1.grid()
		fig.set_layout_engine("tight")
		plt.show()
	return A, kwargs, avg_decode_rate_arr, avg_avg_timing_error_arr


def shotgun_parameterspace(base_kwargs:dict, argname_choisearr_dict, noise_p_array, n_variants, n_runs_per):
	n_permutations = np.prod(np.array([len(argname_choisearr_dict[argname]) for argname in argname_choisearr_dict.keys()]))
	print("{} argument permutations.".format(n_permutations))
	mpr_argtuple_list = list()
	for i_variant in range(n_variants):
		key_kwargs = dict()
		for argname in argname_choisearr_dict.keys():
			n_choises = len(argname_choisearr_dict[argname])
			key_kwargs[argname] = argname_choisearr_dict[argname][np.random.randint(0, n_choises)]
		kwargs = base_kwargs.copy()
		kwargs.update(key_kwargs)
		argtuple = (kwargs, noise_p_array.copy(), n_runs_per)
		mpr_argtuple_list.append(argtuple)

	best_A = -1e10
	best_kwargs = None
	best_key_kwargs = None
	stats_of_best = None
	c = 0
	result_dict = {
		"ts" : dtime.now().isoformat(),
		"base_kwargs":base_kwargs.copy(),
		"noise_p_array":noise_p_array.copy(),
		"n_runs_per":n_runs_per,
		"keyarg_names": tuple(sorted(tuple(argname_choisearr_dict.keys()))),
		"(A,decoderates,keyargs)": list()
	}
	while c < len(mpr_argtuple_list):
		batch = mpr_argtuple_list[c:c+16]
		print("Batch {}-{}/{}.".format(c+1,min(c+16,n_variants),n_variants))
		c += 16
		ret_list, dt_list = mpr_set(f=evaluate_param_kwargs, argtuple_list=batch, ncores=7, Q_or_NS="NS", picklepack=True, verbose=False)
		ret_list = sorted(ret_list, key=lambda k: k[0], reverse=True)
		for ret in ret_list:
			result_dict["(A,decoderates,keyargs)"].append( (ret[0], ret[2], tuple(ret[1][k] for k in result_dict["keyarg_names"])) )
		A, kwargs, avg_decode_rate_arr, avg_avg_timing_error_arr = ret_list[0]
		key_kwargs = {k:v for k,v in kwargs.items() if k in argname_choisearr_dict}
		if A > best_A:
			print("New best with {}: ".format(A), key_kwargs)
			best_A = A
			best_kwargs = kwargs.copy()
			best_key_kwargs = key_kwargs.copy()
			stats_of_best = [avg_decode_rate_arr, avg_avg_timing_error_arr, noise_p_array]
	return best_A, best_kwargs, best_key_kwargs, stats_of_best, result_dict


noiseP_array_9600 = [0.1e-6/9600, 0.02/9600, 0.04/9600, 0.06/9600, 0.08/9600, 0.10/9600, 0.12/9600, 0.14/9600, 0.16/9600, 0.18/9600, 0.2/9600, 0.22/9600, 0.24/9600]
noiseP_array_19200 = [0.1e-6/19200, 0.02/19200, 0.04/19200, 0.06/19200, 0.08/19200, 0.10/19200, 0.12/19200, 0.14/19200, 0.16/19200, 0.18/19200, 0.2/19200, 0.22/19200, 0.24/19200]
base_kwargs_br9600_fo05 = {"baudrate":9600, "n_symbols":8*150, "relative_rate_error":2.5e-5, "f_offset_in_br":0.05, "synch_mode":0,  "mod_idx":0.5, "BT":0.5, "do_prints":False, "do_plots":False}
base_kwargs_br19200_fo05 = {"baudrate":19200, "n_symbols":8*150, "relative_rate_error":2.5e-5, "f_offset_in_br":0.05, "synch_mode":0,  "mod_idx":0.5, "BT":0.5, "do_prints":False, "do_plots":False}

choise_arrays_set_modBT = {
	"sps": 				[13,15,16,17,19,20,21,22,23,25,26],
	"lp_coeff": 		[round(float(x),4) for x in np.linspace(0.7, 1.6, 64) * 0.630],
	"n_decay": 			[int(x) for x in np.linspace(20, 38, 32)],
	"synch_delay_mpr": 	[round(float(x),4) for x in np.linspace(6.0, 32,32)],
}

excellent_9600 = base_kwargs_br9600_fo05.copy()
excellent_19200 = base_kwargs_br19200_fo05.copy()
excellent_9600.update( {
	"sps": 				21,
	"lp_coeff": 		0.630,
	"n_decay": 			30,
	"synch_delay_mpr": 	22.0,
} )
excellent_19200.update( {
	"sps": 				21,
	"lp_coeff": 		0.630,
	"n_decay": 			30,
	"synch_delay_mpr": 	22.0,
} )



if __name__ == '__main__':
	tst_synch_and_decode(sps=21, baudrate=9600, n_symbols=2*1024, relative_rate_error=2.5e-5, noisePpHz=0.0005/9600, f_offset_in_br=0.05, synch_mode=0, mod_idx=0.5, BT=0.5, lp_coeff=0.630, n_decay=24, synch_delay_mpr=8.0, do_prints=True, do_plots=True)

	t000 = time.perf_counter()
	for _ in range(6):
		tst_synch_and_decode(sps=21, baudrate=9600, n_symbols=8*140, relative_rate_error=2.5e-5, noisePpHz=0.05/9600, f_offset_in_br=0.05, synch_mode=0, mod_idx=0.5, BT=0.5, lp_coeff=0.630, n_decay=24, synch_delay_mpr=8.0, do_prints=False, do_plots=False)
	dt_all = (time.perf_counter() - t000) / 6
	print("One run: {} ms".format(  round(1e3*dt_all, 3) ))
	print("        ~{} /s".format(  round(1/dt_all, 1) ))



	A_excellent_9600, _,_,_ = evaluate_param_kwargs(kwargs=excellent_9600, noise_p_array=noiseP_array_9600, n_runs_per=30, do_plots=True)
	A_excellent_19200, _,_,_ = evaluate_param_kwargs(kwargs=excellent_19200, noise_p_array=noiseP_array_19200, n_runs_per=30, do_plots=True)
	print("A-ref-excellent-9600: ", A_excellent_9600)
	print("A-ref-excellent-19200: ", A_excellent_19200)

	_, _, _, _, result_dict_ = shotgun_parameterspace(base_kwargs=base_kwargs_br19200_fo05, argname_choisearr_dict=choise_arrays_set_modBT, noise_p_array=noiseP_array_19200, n_variants=200, n_runs_per=36)
	dpath = "/home/elmore/datasetit/mc_results/"
	letters = "".join(chr(x) for x in np.random.randint(ord("A"), ord("Z"), 4))
	fname = "kuokka_params_mc_result_{}.pkl".format(letters)
	f = open(dpath + fname, "wb")
	f.write(pickle.dumps(result_dict_))
	f.close()
	print("written into: ", dpath+fname)



# A-ref-excellent-9600:  1.9473883311275874e-05
# A-ref-excellent-19200:  9.671845006949406e-06

# br=19200, fo/br=0.05
# New best with 1.0357207371706989e-05:  {'sps': 22, 'lp_coeff': 0.6069512195121951, 'n_decay': 34, 'synch_delay_mpr': 27.80645161290322} (n_lptaps=201)
# New best with 1.0548728352701363e-05:  {'sps': 21, 'lp_coeff': 0.6069512195121951, 'n_decay': 37, 'synch_delay_mpr': 26.96774193548387} (n_lptaps=201)
# New best with 1.0288917220800924e-05:  {'sps': 15, 'lp_coeff': 0.639,              'n_decay': 33, 'synch_delay_mpr': 11.87096774193548} (n_lptaps=201)
# New best with 1.0341279227599047e-05:  {'sps': 21, 'lp_coeff': 0.5579999999999999, 'n_decay': 36, 'synch_delay_mpr': 12.70967741935484} (n_lptaps=201)
# New best with 1.0416382038106999e-05:  {'sps': 20, 'lp_coeff': 0.63,               'n_decay': 38, 'synch_delay_mpr': 12.70967741935484} (n_lptaps=201)
# New best with 1.0349004116960635e-05:  {'sps': 17, 'lp_coeff': 0.639, 'n_decay': 36, 'synch_delay_mpr': 16.9032} (n_lptaps=161)
# New best with 1.0395775778740248e-05:  {'sps': 22, 'lp_coeff': 0.639, 'n_decay': 37, 'synch_delay_mpr': 22.7742} (n_lptaps=161)







