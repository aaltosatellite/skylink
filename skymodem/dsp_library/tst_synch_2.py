import os.path
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
from tst_synch_1 import synch_and_decode_experiment
#def f_tria(x,m):
#	x2 = x % m
#	x2 = x2 + (m-x2*2)*(x2>(m/2))
#	return x2



def result_analysis(fpath_list, x_axis, filters_name_span):
	results = list()
	x_coords = list()
	y_coords = list()
	for fpath in fpath_list:
		assert os.path.isfile(fpath)
		f = open(fpath, "rb")
		result_d = pickle.loads(f.read())
		f.close()
		assert type(result_d) == dict
		base_kwargs = result_d["base_kwargs"]
		optparam_names = result_d["optparam_names"]
		#idx_x_value = optparam_names.index(x_axis)
		for A,decoderates,optparams in result_d["(A,decoderates,optparams)"]:
			optparam_d = dict(zip(optparam_names, optparams))
			params_d = base_kwargs.copy()
			params_d.update(optparam_d)
			results.append((params_d, A))
	print("Loaded {} results.".format(len(results)))
	for (params_d, A) in results:
		filtered = False
		for name,span in filters_name_span:
			if not (span[0] <= params_d[name] <= span[1]):
				filtered = True
		if filtered:
			continue
		x = params_d[x_axis]
		x_coords.append(x)
		y_coords.append(A)

	fig= plt.figure(figsize=(13,13))
	ax1 = fig.add_subplot(111)

	ax1.scatter( x_coords, y_coords, marker="x")
	ax1.grid()

	fig.set_layout_engine("tight")
	plt.show()











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
			sample_timing_errors, avg_timing_error, corrmax, n_symbols = synch_and_decode_experiment(**_kwgs)
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


def shotgun_parameterspace(base_kwargs:dict, optparam_choisearr_dict, noise_p_array, n_variants, n_runs_per):
	n_permutations = np.prod(np.array([len(optparam_choisearr_dict[argname]) for argname in optparam_choisearr_dict.keys()]))
	print("{} argument permutations.".format(n_permutations))
	mpr_argtuple_list = list()
	for i_variant in range(n_variants):
		optparam_kwargs = dict()
		for argname in optparam_choisearr_dict.keys():
			n_choises = len(optparam_choisearr_dict[argname])
			optparam_kwargs[argname] = optparam_choisearr_dict[argname][np.random.randint(0, n_choises)]
		kwargs = base_kwargs.copy()
		kwargs.update(optparam_kwargs)
		argtuple = (kwargs, noise_p_array.copy(), n_runs_per)
		mpr_argtuple_list.append(argtuple)

	best_A = -1e10
	best_kwargs = None
	best_optparam_kwargs = None
	stats_of_best = None
	t_start = time.perf_counter()
	c = 0
	result_dict = {
		"ts" : dtime.now().isoformat(),
		"base_kwargs":base_kwargs.copy(),
		"noise_p_array":noise_p_array.copy(),
		"n_runs_per":n_runs_per,
		"optparam_names": tuple(sorted(tuple(optparam_choisearr_dict.keys()))),
		"(A,decoderates,optparams)": list()
	}
	while c < len(mpr_argtuple_list):
		batch = mpr_argtuple_list[c:c+16]
		avg_speed = c / (time.perf_counter() - t_start)
		print("Batch {}-{}/{}.".format(c+1,min(c+16,n_variants),n_variants))
		print("~{} variants/s".format(round(avg_speed, 1)))
		c += 16
		ret_list, dt_list = mpr_set(f=evaluate_param_kwargs, argtuple_list=batch, ncores=7, Q_or_NS="NS", picklepack=True, verbose=False)
		ret_list = sorted(ret_list, key=lambda k: k[0], reverse=True)
		for ret in ret_list:
			result_dict["(A,decoderates,optparams)"].append( (ret[0], ret[2], tuple(ret[1][k] for k in result_dict["optparam_names"])) )
		A, kwargs, avg_decode_rate_arr, avg_avg_timing_error_arr = ret_list[0]
		optparam_kwargs = {k:v for k,v in kwargs.items() if k in optparam_choisearr_dict}
		if A > best_A:
			print("New best with {}: ".format(A), optparam_kwargs)
			best_A = A
			best_kwargs = kwargs.copy()
			best_optparam_kwargs = optparam_kwargs.copy()
			stats_of_best = [avg_decode_rate_arr, avg_avg_timing_error_arr, noise_p_array]
	return best_A, best_kwargs, best_optparam_kwargs, stats_of_best, result_dict


noiseP_array_9600 = [0.1e-6/9600, 0.02/9600, 0.04/9600, 0.06/9600, 0.08/9600, 0.10/9600, 0.12/9600, 0.14/9600, 0.16/9600, 0.18/9600, 0.2/9600, 0.22/9600, 0.24/9600]
noiseP_array_19200 = [0.1e-6/19200, 0.02/19200, 0.04/19200, 0.06/19200, 0.08/19200, 0.10/19200, 0.12/19200, 0.14/19200, 0.16/19200, 0.18/19200, 0.2/19200, 0.22/19200, 0.24/19200]
base_kwargs_br9600_fo05 = {"baudrate":9600, "n_symbols":8*150, "relative_rate_error":2.5e-5, "f_offset_in_br":0.05, "mod_idx":0.5, "BT":0.5, "do_prints":False, "do_plots":False}
base_kwargs_br19200_fo05 = {"baudrate":19200, "n_symbols":8*150, "relative_rate_error":2.5e-5, "f_offset_in_br":0.05, "mod_idx":0.5, "BT":0.5, "do_prints":False, "do_plots":False}

optparam_arrays_set_modBT = {
	"sps": 				[3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,25,26,27,28],
	"lp_coeff": 		[round(float(x),4) for x in np.linspace(0.4, 1.7, 64) * 0.630],
	"n_decay": 			[int(x) for x in np.linspace(2, 38, 40)],
	"synch_delay_mpr": 	[round(float(x),4) for x in np.linspace(1.0, 32.0,64)],
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
	fpathss = [	"/home/elmore/datasetit/mc_results/kuokka_params_mc_result_GRFL.pkl",
				"/home/elmore/datasetit/mc_results/kuokka_params_mc_result_PDBR.pkl",
				"/home/elmore/datasetit/mc_results/kuokka_params_mc_result_GWUF.pkl",
				"/home/elmore/datasetit/mc_results/kuokka_params_mc_result_PQFV.pkl",
				"/home/elmore/datasetit/mc_results/kuokka_params_mc_result_IUXR.pkl",
				"/home/elmore/datasetit/mc_results/kuokka_params_mc_result_QVHR.pkl",
				   ]
	result_analysis(fpath_list=fpathss, x_axis="synch_delay_mpr", filters_name_span=[("lp_coeff",(0.5,0.7)),  ("n_decay",(25,100)), ("sps",(16, 110))])

	synch_and_decode_experiment(sps=21, baudrate=9600, n_symbols=2*1024, relative_rate_error=2.5e-5, noisePpHz=0.0005/9600, f_offset_in_br=0.05, mod_idx=0.5, BT=0.5, lp_coeff=0.630, n_decay=24, synch_delay_mpr=8.0, do_prints=True, do_plots=True)

	t000 = time.perf_counter()
	for _ in range(6):
		synch_and_decode_experiment(sps=21, baudrate=9600, n_symbols=8*140, relative_rate_error=2.5e-5, noisePpHz=0.05/9600, f_offset_in_br=0.05, mod_idx=0.5, BT=0.5, lp_coeff=0.630, n_decay=24, synch_delay_mpr=8.0, do_prints=False, do_plots=False)
	dt_all = (time.perf_counter() - t000) / 6
	print("One run: {} ms".format(  round(1e3*dt_all, 3) ))
	print("        ~{} /s".format(  round(1/dt_all, 1) ))



	A_excellent_9600, _,_,_ = evaluate_param_kwargs(kwargs=excellent_9600, noise_p_array=noiseP_array_9600, n_runs_per=30, do_plots=True)
	A_excellent_19200, _,_,_ = evaluate_param_kwargs(kwargs=excellent_19200, noise_p_array=noiseP_array_19200, n_runs_per=30, do_plots=True)
	print("A-ref-excellent-9600: ", A_excellent_9600)
	print("A-ref-excellent-19200: ", A_excellent_19200)

	_, _, _, _, result_dict_ = shotgun_parameterspace(base_kwargs=base_kwargs_br19200_fo05, optparam_choisearr_dict=optparam_arrays_set_modBT, noise_p_array=noiseP_array_19200, n_variants=200, n_runs_per=40)
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







