import numpy as np
import time
from kuokka.lib_tools import make_samples, radionoise
from kuokka.lib_symsynching import create_classic_JPL_statemx
from kuokka.lib_demodulation import demodulation_sequence, create_DSD_statemx
from matplotlib import pyplot as plt
from mtools.tools_system import mpr_set
import os
import pickle


## ===========================================================================================================================================================================================
## ===========================================================================================================================================================================================
def demodulation_experiment(noisepower, rx_params, baudrate, nbits, f_offset, do_print, do_plot):
	assert nbits > 10
	assert abs(f_offset) < 0.5
	assert baudrate
	assert type(noisepower) in (float, np.float64)
	assert noisepower >= 0
	assert params_valid(params_d=rx_params)
	mod_index 		= rx_params["mod_index"]
	BT 				= rx_params["BT"]
	sps 			= rx_params["sps"]
	JPLdecay 		= rx_params["JPLdecay"]
	synch_delay_mpr = rx_params["synch_delay_mpr"]
	lp_cutoff_coeff = rx_params["lp_cutoff_coeff"]
	lp_ntaps 		= rx_params["lp_ntaps"]
	for x in sps,lp_ntaps:
		assert type(x) == int
		assert x >= 0
	for x in JPLdecay,synch_delay_mpr,lp_cutoff_coeff:
		assert type(x) == float
		assert x >= 0
	sr 				= sps*baudrate
	lp_cutoff 		= lp_cutoff_coeff*baudrate/sr
	artif_margin 	= 2*int(JPLdecay*sps + synch_delay_mpr*sps + 2*sps)

	t00 = time.perf_counter()
	bitarr_actual 			= np.random.randint(0,2, nbits)*2 - 1
	bitarr_actual[0:16] 	= np.array([1,-1,]*8)
	bitarr_actual[16:16+8] 	= np.array([1,]*7 + [-1,])
	signal 					= make_samples(sps_f=sps, bitstring=bitarr_actual, f_offset=f_offset, power=1, mod_index=mod_index, shaper_mode=0, shaper_BT_prod=BT, shaper_n_taps=sps*10+1)
	t_datagen1 = time.perf_counter() - t00

	t00 = time.perf_counter()
	nsignal 		= len(signal)
	nnoise 			= len(signal) + np.random.randint(0, sps*2)
	_noisecap 		= np.zeros(nnoise, dtype=np.float64) + 1j*np.zeros(nnoise, dtype=np.float64)
	samples 		= np.concatenate( (_noisecap, signal, _noisecap) )
	nsamples 		= len(samples)
	samples 		= samples + radionoise(n=nsamples, sr=sr, W_per_Hz=noisepower)
	t_datagen2 = time.perf_counter() - t00

	assert artif_margin < nnoise
	#print("\tnsignal/nsamps:  {}".format( round(nsignal / nsamples, 4) ))
	#print("\tmargin/nsamps:   {}".format( round(artif_margin / nsamples, 4) ))

	t00 = time.perf_counter()
	center_f_array 	= np.zeros(nsamples) -1
	center_f_array[nnoise-artif_margin : nnoise+nsignal+artif_margin] = f_offset
	dmd_arr 		= np.zeros(nsamples, dtype=np.float64)
	synch_arr 		= np.zeros((nsamples,3), dtype=np.int64)
	bitarr 			= np.zeros(3*int(nsamples/sps))
	bitfarr 		= np.zeros(3*int(nsamples/sps), dtype=np.float64)
	t_arraygen = time.perf_counter() - t00

	t00 = time.perf_counter()
	JPLstatemx 		= create_classic_JPL_statemx(N_eps=sps, n_decay=JPLdecay)
	DSD_statemx 	= create_DSD_statemx(lp_ntaps=lp_ntaps, lp_cutoff=lp_cutoff, synch_delay_mpr_f=synch_delay_mpr, sps_f=sps)
	t_stategen = time.perf_counter() - t00

	t00 = time.perf_counter()
	dmd_head, bit_head = demodulation_sequence(rs_arr=samples, centerf_arr=center_f_array, i_rs0=0, nsamples=nsamples, dmd_arr=dmd_arr,
						  synch_arr=synch_arr, dmdsynch_head0=0, JPLstatemx=JPLstatemx, demodmx=DSD_statemx, bitarr=bitarr, bitfarr=bitfarr, bit_head0=0)
	t_call = time.perf_counter() - t00

	speed = nsamples / t_call
	overmatch = speed / sr
	budget_fraction	= (1/overmatch) / 0.5

	t00 = time.perf_counter()
	bitarr = bitarr[:bit_head]
	bitcorr = np.correlate(bitarr, bitarr_actual)
	maxcorr_arg = np.argmax(bitcorr)
	maxcorr = bitcorr[maxcorr_arg]

	aligned_result = np.int64(bitarr[maxcorr_arg:maxcorr_arg+nbits])
	match_arr = aligned_result * bitarr_actual[0:len(aligned_result)]
	match_sum = np.sum( match_arr > 0 )

	start_form = np.array( [1,-1, 1,1,1,1,1,1,1, -1] )
	start_form_correlation = np.correlate( aligned_result[:16+8+4], start_form )
	start_form_match_sum = np.max(start_form_correlation)
	start_form_match_sum = start_form_match_sum + (len(start_form) - start_form_match_sum)//2
	t_corr = time.perf_counter() - t00

	if do_print:
		print("T-datagen1:    {} ms  ({}k samples)".format( round( 1e3*t_datagen1, 2 ), int(nsamples*1e-3) ))
		print("T-datagen2:    {} ms".format( round( 1e3*t_datagen2, 2 ) ))
		print("T-arraygen:    {} ms".format( round( 1e3*t_arraygen, 2 ) ))
		print("T-stategen:    {} ms".format( round( 1e3*t_stategen, 2 ) ))
		print("T-run:         {} ms".format( round( 1e3*t_call, 2 ) ))
		print("T-corr:        {} ms".format( round( 1e3*t_corr, 2 ) ))
		#print("(Got {} bits, vs {})".format(bit_head, int(nsamples/sps)  ))
		print("Maxcorr:         {}".format(maxcorr))
		print("Match sum:       {}".format(match_sum))
		print("start match sum: {}".format(start_form_match_sum))
		print("speed:         {} Ms/s".format( round( 1e-6 * speed, 2 ) ))
		print("overmatch:     {} Ms/s".format( round( overmatch, 2 ) ))
		print("budget use:    {} %".format( round( 100*budget_fraction, 2 ) ))
		print("")

	if do_plot:
		fig = plt.figure(figsize=(14,11))
		ax1 = fig.add_subplot(111)
		ax1.plot(synch_arr[:,0])
		ax1.grid()
		fig.set_layout_engine("tight")
		plt.show()

	return maxcorr, match_sum, start_form_match_sum, (t_call,speed,overmatch,budget_fraction)
## ===========================================================================================================================================================================================
## ===========================================================================================================================================================================================












## ===========================================================================================================================================================================================
## ===========================================================================================================================================================================================
def params_scalar_gen(typespec, span):
	assert typespec in ("i", "r", "i_odd", "i_even", "list")
	if not (typespec == "list"):
		assert len(span) in (2,3)
		assert span[1] >= span[0]
		assert type(span[0]) == type(span[1])
		assert type(span[0]) in (float, int)
	else:
		assert len(span) >= 1
		for i in range(len(span)):
			assert type(span[0]) == type(span[i])
	if typespec == "list":
		return span[ np.random.randint(0, len(span)) ]
	if typespec == "i":
		assert len(span) == 2
		assert type(span[0]) == int
		return np.random.randint(span[0], span[1]+1)
	if typespec == "i_odd":
		assert len(span) == 2
		assert type(span[0]) == int
		assert (span[0]%2) == 1
		assert (span[1]%2) == 1
		i = np.random.randint(span[0], span[1]+1)
		i = i - (i%2) + 1
		assert (i%2) == 1
		return i
	if typespec == "i_even":
		assert len(span) == 2
		assert type(span[0]) == int
		assert (span[0]%2) == 0
		assert (span[1]%2) == 0
		i = np.random.randint(span[0], span[1]+1)
		i = i - (i%2)
		assert (i%2) == 0
		return i
	if typespec == "r":
		assert len(span) == 3
		ndiv = span[2]
		assert type(ndiv) == int
		assert (ndiv == -1) or (ndiv > 1)
		gap = span[1] - span[0]
		if ndiv == -1:
			return span[0] + np.random.random()*gap
		i = np.random.randint(0, ndiv)
		return np.linspace(span[0], span[1], ndiv)[i]


def params_generate(param_space_spec):
	params = dict()
	for key, val in param_space_spec.items():
		typespec, span = val
		p = params_scalar_gen(typespec=typespec, span=span)
		params[key] = p
	assert params_valid(params)
	return params


def params_compute_space_size(param_space_spec):
	assert params_valid(params_generate(param_space_spec))  # ensures validity
	size = 1
	for name, val in param_space_spec.items():
		typespec, span = val
		if (typespec == "r") and (span[2] == -1):
			return -1
		if typespec == "i":
			size = size * ((span[1]-span[0]) + 1)
			continue
		if typespec in ("i_even", "i_odd"):
			size = size * (1 + (span[1]-span[0])//2)
			continue
		if typespec == "r":
			assert type(span[2]) == int
			assert span[2] > 0
			size = size * span[2]
			continue
		if typespec == "list":
			size = size * len( span )
			continue
		raise AssertionError("Unknown typespec")
	return size


def params_dict_to_hashable(params_dict):
	sorted_keys = sorted( list( params_dict.keys() ) )
	ll = list()
	for k in sorted_keys:
		assert type(k) == str
		ll.append(k)
		ll.append(params_dict[k])
	return tuple(ll)


def params_hashable_to_dict(tup):
	assert type(tup) == tuple
	assert len(tup) == 10
	dd = dict()
	assert tup[0] == "sps"
	dd["sps"] = tup[1]
	assert tup[2] == "JPLdecay"
	dd["JPLdecay"] = tup[3]
	assert tup[4] == "synch_delay_mpr"
	dd["synch_delay_mpr"] = tup[5]
	assert tup[6] == "lp_cutoff_coeff"
	dd["lp_cutoff_coeff"] = tup[7]
	assert tup[8] == "lp_ntaps"
	dd["lp_ntaps"] = tup[9]


def params_valid(params_d):
	try:
		assert sorted(list(params_d.keys())) == sorted(list(default_rx_params.keys()))
		assert type(params_d["sps"]) == int
		assert 1 < params_d["sps"] < 125
		assert type(params_d["JPLdecay"]) == float
		assert 1.0 <= params_d["JPLdecay"] < 200.0
		assert type(params_d["synch_delay_mpr"]) == float
		assert 0.0 <= params_d["synch_delay_mpr"] < 100.0
		assert type(params_d["lp_cutoff_coeff"]) == float
		assert 0.1 <= params_d["lp_cutoff_coeff"] <= 5.0
		assert type(params_d["lp_ntaps"]) == int
		assert (params_d["lp_ntaps"] % 2) == 1
		assert 61 <= params_d["lp_ntaps"] <= 201
		assert (0.2 <= params_d["BT"] <= 8.0) or (params_d["BT"] == -1)
		assert 0.3 <= params_d["mod_index"] <= 4.0
		return True
	except:
		pass
	return False


def surf_integral(x_arr, y_arr):
	A = 0
	assert len(x_arr) > 1
	assert len(x_arr) == len(y_arr)
	for i in range(len(x_arr)-1):
		assert x_arr[i+1] > x_arr[i]
		dx = x_arr[i+1] - x_arr[i]
		A += dx * (y_arr[i] + y_arr[i+1])*0.5
	return A


def evaluate_params(param_d, noise_arr, n_per_point,  baudrate,):
	avg_arr = np.zeros(len(noise_arr), dtype=np.float64)
	std_arr = np.zeros(len(noise_arr), dtype=np.float64)
	n_noise = len(noise_arr)
	for i_noise in range(n_noise):
		Pnoise = noise_arr[i_noise]
		criterions = np.zeros(n_per_point, dtype=np.float64)
		for i_rep in range(n_per_point):
			maxcorr_, match_sum_, start_form_match_sum_, _ = demodulation_experiment(noisepower=Pnoise, rx_params=param_d, baudrate=baudrate, nbits=1000, f_offset=0.0, do_print=False, do_plot=False)
			criterion = ((match_sum_ / 1000) * 2 - 1.0)**2
			criterions[i_rep] = criterion
		avg_arr[i_noise] = np.average(criterions)
		std_arr[i_noise] = np.std(criterions)
	A = surf_integral(x_arr=noise_arr, y_arr=avg_arr)
	return float(A), avg_arr, std_arr
## ===========================================================================================================================================================================================
## ===========================================================================================================================================================================================








default_rx_params = dict()
default_rx_params["sps"] 				= 27		# param ~
default_rx_params["JPLdecay"] 			= 16.0		# param ~
default_rx_params["synch_delay_mpr"] 	= 8.0		# param ~
default_rx_params["lp_cutoff_coeff"] 	= 0.6		# param !
default_rx_params["lp_ntaps"] 			= 181		# param ~
default_rx_params["BT"] 				= 0.8		# param !
default_rx_params["mod_index"] 			= 0.5		# param !


dream_params = dict()
dream_params["sps"] 				= 25		# param ~
dream_params["JPLdecay"] 			= 28.0		# param ~
dream_params["synch_delay_mpr"] 	= 22.0		# param ~
dream_params["lp_cutoff_coeff"] 	= 0.6		# param !
dream_params["lp_ntaps"] 			= 121		# param ~
dream_params["BT"] 					= 1.0		# param !
dream_params["mod_index"] 			= 1.0		# param !

savior_params = dict()
savior_params["sps"] 				= 21		# param ~
savior_params["JPLdecay"] 			= 32.0		# param ~
savior_params["synch_delay_mpr"] 	= 22.0		# param ~
savior_params["lp_cutoff_coeff"] 	= 0.625		# param !
savior_params["lp_ntaps"] 			= 121		# param ~
savior_params["BT"] 				= -1.0		# param !
savior_params["mod_index"] 			= 0.7		# param !


noise_array_1	= np.array((0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2))

rx_param_space_1 = dict()
rx_param_space_1["sps"] 				= ("i_odd", (15, 31))	#[13,15,17,19,21,23,25,27,29,31]
rx_param_space_1["JPLdecay"] 			= ("list", (2.0, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0, 18.0, 20.0, 22.0, 24.0, 28.0, 32.0))
rx_param_space_1["synch_delay_mpr"] 	= ("list", (0.0, 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0, 18.0, 20.0, 22.0, 24.0))
rx_param_space_1["lp_cutoff_coeff"] 	= ("list", (0.4, 0.45, 0.5, 0.55, 0.575, 0.6, 0.625, 0.65, 0.675, 0.7, 0.725, 0.75, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4))
rx_param_space_1["lp_ntaps"] 			= ("list", (61, 81, 101, 121, 141, 161, 181, 201))
rx_param_space_1["BT"] 					= ("list", (0.6, 0.8, 1.0, 1.2, 1.4, 1.6))
rx_param_space_1["mod_index"] 			= ("list", (0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5))

rx_param_space_1_2 = dict()
rx_param_space_1_2["sps"] 				= ("i_odd", (19, 27))	#[13,15,17,19,21,23,25,27,29,31]
rx_param_space_1_2["JPLdecay"] 			= ("list", (18.0, 20.0, 22.0, 24.0, 28.0, 32.0))
rx_param_space_1_2["synch_delay_mpr"] 	= ("list", (20.0, 22.0, 24.0))
rx_param_space_1_2["lp_cutoff_coeff"] 	= ("list", (0.4, 0.45, 0.5, 0.55, 0.575, 0.6, 0.625, 0.65, 0.675, 0.7, 0.725, 0.75, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4))
rx_param_space_1_2["lp_ntaps"] 			= ("list", (121,))
rx_param_space_1_2["BT"] 				= ("list", (0.5, 0.5, 0.6, 0.8, 1.0, 1.2,)) # 1.4, 1.6))  #0.5 is addition.
rx_param_space_1_2["mod_index"] 		= ("list", (0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5))

rx_param_space_1_3 = dict()
rx_param_space_1_3["sps"] 				= ("i_odd", (19, 25))	#[13,15,17,19,21,23,25,27,29,31]
rx_param_space_1_3["JPLdecay"] 			= ("list", (18.0, 20.0, 22.0, 24.0, 28.0, 32.0))
rx_param_space_1_3["synch_delay_mpr"] 	= ("list", (20.0, 22.0, 24.0))
rx_param_space_1_3["lp_cutoff_coeff"] 	= ("list", (0.4, 0.45, 0.5, 0.525, 0.55, 0.575, 0.6, 0.625, 0.65, 0.675, 0.7, 0.725, 0.75, 0.8, 0.85, 0.9, 1.0, 1.2, 1.3, 1.4, 1.5, 1.6, 1.8, 2.0, 2.1, 2.2))
rx_param_space_1_3["lp_ntaps"] 			= ("list", (121,))
rx_param_space_1_3["BT"] 				= ("list", (0.5,-1.0)) #-1.0 removed
rx_param_space_1_3["mod_index"] 		= ("list", (0.6, 0.65, 0.7, 0.707, 0.725, 0.75, 0.8, 0.85, 0.9, 0.95, 1.0, 1.05, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.8, 3.0))


rx_param_space_1_4 = dict()
rx_param_space_1_4["sps"] 				= ("i_odd", (19, 25))	#[13,15,17,19,21,23,25,27,29,31]
rx_param_space_1_4["JPLdecay"] 			= ("list", (18.0, 20.0, 22.0, 24.0, 28.0, 32.0))
rx_param_space_1_4["synch_delay_mpr"] 	= ("list", (20.0, 22.0, 24.0))
rx_param_space_1_4["lp_cutoff_coeff"] 	= ("list", (0.5, 0.525, 0.55, 0.575, 0.6, 0.625, 0.65, 0.675, 0.7, 0.725, 0.75, 0.8))
rx_param_space_1_4["lp_ntaps"] 			= ("list", (121,))
rx_param_space_1_4["BT"] 				= ("list", (-1.0)) #0.5 removed
rx_param_space_1_4["mod_index"] 		= ("list", (0.6, 0.65, 0.675, 0.7, 0.707, 0.725, 0.75, 0.8, 0.85, 0.9, 0.95, 1.0, 2.0, 3.0))






rx_param_space_2 = dict()
rx_param_space_2["sps"] 				= ("i_odd", (15, 31))	#[13,15,17,19,21,23,25,27,29,31]
rx_param_space_2["JPLdecay"] 			= ("r",     (2.0, 32.0, -1))
rx_param_space_2["synch_delay_mpr"] 	= ("r",     (0.0, 24.0, -1))
rx_param_space_2["lp_cutoff_coeff"] 	= ("r",     (0.4, 1.4,  -1))
rx_param_space_2["lp_ntaps"] 			= ("list",  (61, 81, 101, 121, 141, 161, 181, 201))
rx_param_space_2["BT"] 					= ("r",     (0.6, 1.6,  -1))
rx_param_space_2["mod_index"] 			= ("r",     (0.5, 1.3,  -1))







def save_a_result_set(baudrate, noise_arr, n_rep, result_list, dpath, fname_base):
	assert os.path.isdir(dpath)
	assert type(baudrate) in (float, int)
	assert baudrate > 100
	assert type(noise_arr) == np.ndarray
	assert noise_arr.dtype == np.float64
	assert len(noise_arr) > 1
	assert len(noise_arr) < 100
	assert type(n_rep) == int
	assert n_rep > 1
	assert type(result_list) == list
	for res in result_list:
		assert len(res) == 4
		assert type(res[0]) == dict			# params
		assert type(res[1]) == float		# A
		assert type(res[2]) == np.ndarray	# avg_arr
		assert type(res[3]) == np.ndarray	# std_arr
		assert len(res[2]) == len(noise_arr)
		assert len(res[3]) == len(noise_arr)
	report_d = dict()
	report_d["noise_arr"] = noise_arr
	report_d["n_rep"] = n_rep
	report_d["baudrate"] = baudrate
	report_d["results"] = result_list
	while True:
		fpath = os.path.join(dpath, fname_base + "-"+str(np.random.randint(0, 10000)))
		if not os.path.exists(fpath):
			break
	f = open(fpath, "wb")
	f.write(pickle.dumps(report_d))
	f.close()










def tst_():
	import sys
	dictsize = sys.getsizeof(default_rx_params)
	print("size of dict: {}".format(dictsize))
	space_size_1 = params_compute_space_size(param_space_spec=rx_param_space_1)
	print("Parameter space 1 size:            {}".format(space_size_1))
	print("Parameter space size * dict size:  {} M".format(space_size_1 *dictsize / 1e6 ))
	print("Iteration time:          {} h".format( round(space_size_1 * 0.025 * 100 / (7*3600), 1)  ))
	print("")

	demodulation_experiment(noisepower=0.01 / 9600, rx_params=default_rx_params, baudrate=9600, nbits=1000, f_offset=0.1, do_print=True, do_plot=False)

	params = savior_params.copy()
	params["mod_index"] 		= 0.9
	params["lp_cutoff_coeff"] 	= 0.9
	t0 = time.perf_counter()
	demodulation_experiment(noisepower=2.0 / 9600, rx_params=savior_params, baudrate=960, nbits=1000, f_offset=0.15, do_print=True, do_plot=False)
	demodulation_experiment(noisepower=0.2 / 9600, rx_params=savior_params, baudrate=9600, nbits=1000, f_offset=0.15, do_print=True, do_plot=False)
	T_call = (time.perf_counter() - t0) / 2

	print("T-call:      {} ms".format( round(1e3*T_call, 2) ))
	print("call speed:  {} /s".format( round(1/T_call, 1) ))








def tst2():
	noise_arr = np.linspace(0.0, 0.7, 12) * 1/9600

	A_default, avg_arr_default, _ = evaluate_params(param_d=default_rx_params, noise_arr=noise_arr, n_per_point=72, baudrate=9600)
	print("\tA of default: {}".format(A_default))

	A_dream, avg_arr_dream, _ = evaluate_params(param_d=dream_params, noise_arr=noise_arr, n_per_point=72, baudrate=9600)
	print("\tA of dream: {}".format(A_dream))

	A_savior, avg_arr_savior, _ = evaluate_params(param_d=savior_params, noise_arr=noise_arr, n_per_point=72, baudrate=9600)
	print("\tA of savior: {}".format(A_savior))

	fig = plt.figure(figsize=(14,11))
	ax1 = fig.add_subplot(111)
	ax1.plot(noise_arr, avg_arr_default)
	ax1.plot(noise_arr, avg_arr_dream)
	ax1.plot(noise_arr, avg_arr_savior)
	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()

	argv_list = list()
	n_rep = 100
	baudrate = 9600
	for ii in range(140):
		params = params_generate(param_space_spec=rx_param_space_1_3)
		argv_list.append( [params, noise_arr, n_rep, baudrate] )

	print("Starting MPR set")
	ret_list, dt_list = mpr_set(f=evaluate_params, argtuple_list=argv_list, ncores=8, Q_or_NS="NS", picklepack=True, verbose=True)
	assert len(ret_list) == len(argv_list)

	results = list()
	for ii in range(len(argv_list)):
		assert type(ret_list[ii][0]) == float
		assert type(ret_list[ii][1]) == np.ndarray
		assert type(ret_list[ii][2]) == np.ndarray
		results.append( [argv_list[ii][0], ret_list[ii][0], ret_list[ii][1], ret_list[ii][2] ] )
	results = sorted(results, key=lambda k: k[1], reverse=True)
	assert results[0][1] > results[1][1]


	for i_res in range(6):
		print("A: {}".format( round(results[i_res][1], 9) ))
		print("n: {}".format( n_rep ))
		dbest = results[i_res][0]
		for key in dbest.keys():
			print("{} : {} ,".format( key, dbest[key]))
		print("")
		print("")


	colors = ("blue", "orange", "green", "red")
	fig = plt.figure(figsize=(14,11))
	ax1 = fig.add_subplot(111)
	ax1.plot(noise_arr, avg_arr_default, marker="x", color="black")
	ax1.plot(noise_arr, avg_arr_dream, marker="x", color="black")
	ax1.plot(noise_arr, avg_arr_savior, marker="x", color="red")
	for i in range(3):
		color = colors[i]
		ax1.plot(noise_arr, results[i][2], color=color)
		ax1.plot(noise_arr, results[i][2] + 1.96*results[i][3] / np.sqrt(n_rep) , linestyle=":", color=color)
		ax1.plot(noise_arr, results[i][2] - 1.96*results[i][3] / np.sqrt(n_rep), linestyle=":", color=color)
	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()







def tst3():
	params_0 = dict()
	params_0["sps"] 				= 27		# param ~
	params_0["JPLdecay"] 			= 28.0		# param ~
	params_0["synch_delay_mpr"] 	= 20.0		# param ~
	params_0["lp_cutoff_coeff"] 	= 0.6		# param !
	params_0["lp_ntaps"] 			= 121		# param ~
	params_0["BT"] 					= 0.5		# param !
	params_0["mod_index"] 			= 0.5		# param !

	params_1 = dict()
	params_1["sps"] 				= 27		# param ~
	params_1["JPLdecay"] 			= 28.0		# param ~
	params_1["synch_delay_mpr"] 	= 20.0		# param ~
	params_1["lp_cutoff_coeff"] 	= 0.6		# param !
	params_1["lp_ntaps"] 			= 121		# param ~
	params_1["BT"] 					= 0.5		# param !
	params_1["mod_index"] 			= 1.0		# param !

	params_2 = dict()
	params_2["sps"] 				= 27		# param ~
	params_2["JPLdecay"] 			= 28.0		# param ~
	params_2["synch_delay_mpr"] 	= 20.0		# param ~
	params_2["lp_cutoff_coeff"] 	= 0.6		# param !
	params_2["lp_ntaps"] 			= 121		# param ~
	params_2["BT"] 					= 1.0		# param !
	params_2["mod_index"] 			= 1.0		# param !

	params_3 = dict()
	params_3["sps"] 				= 27		# param ~
	params_3["JPLdecay"] 			= 28.0		# param ~
	params_3["synch_delay_mpr"] 	= 20.0		# param ~
	params_3["lp_cutoff_coeff"] 	= 0.6		# param !
	params_3["lp_ntaps"] 			= 121		# param ~
	params_3["BT"] 					= -1.0		# param !
	params_3["mod_index"] 			= 1.0		# param !

	params_4 = dict()
	params_4["sps"] 				= 21		# param ~
	params_4["JPLdecay"] 			= 32.0		# param ~
	params_4["synch_delay_mpr"] 	= 22.0		# param ~
	params_4["lp_cutoff_coeff"] 	= 0.625		# param !
	params_4["lp_ntaps"] 			= 121		# param ~
	params_4["BT"] 					= -1.0		# param !
	params_4["mod_index"] 			= 0.7		# param !

	noise_arr = np.linspace(0.0, 0.7, 12) * 1/9600

	print("Eval 0")
	A0, avg_arr0, std_arr0 = evaluate_params(param_d=params_0, noise_arr=noise_arr, n_per_point=100, baudrate=9600)
	print("Eval 1")
	A1, avg_arr1, std_arr1 = evaluate_params(param_d=params_1, noise_arr=noise_arr, n_per_point=100, baudrate=9600)
	print("Eval 2")
	A2, avg_arr2, std_arr2 = evaluate_params(param_d=params_2, noise_arr=noise_arr, n_per_point=100, baudrate=9600)
	print("Eval 3")
	A3, avg_arr3, std_arr3 = evaluate_params(param_d=params_3, noise_arr=noise_arr, n_per_point=100, baudrate=9600)
	print("Eval 4")
	A4, avg_arr4, std_arr4 = evaluate_params(param_d=params_4, noise_arr=noise_arr, n_per_point=100, baudrate=9600)

	fig = plt.figure(figsize=(14,11))
	ax1 = fig.add_subplot(111)

	ax1.plot(noise_arr, avg_arr0, label="BT = 0.5, mod_idx = 0.5")
	ax1.plot(noise_arr, avg_arr1, label="BT = 0.5, mod_idx = 1.0")
	ax1.plot(noise_arr, avg_arr2, label="BT = 1.0, mod_idx = 1.0")
	ax1.plot(noise_arr, avg_arr3, label="BT = inf, mod_idx = 1.0")
	ax1.plot(noise_arr, avg_arr4, label="BT = inf, mod_idx = 0.7")
	ax1.grid()
	ax1.legend()

	fig.set_layout_engine("tight")
	plt.show()








tst_()
tst2()
#tst3()





























