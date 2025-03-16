import numpy as np
from kuokka.lib_framing import frame_packet
from kuokka.lib_reedsolomon import get_default_rs, RS_MAX_ENCODED_LEN, RS_MAX_PL_LEN, RS_MIN_ENCODED_LEN
from kuokka.lib_tools import DEFAULT_SYNCHWORD, DEFAULT_SYNCHWORD_BITS, DEFAULT_SYNCHWORD_LEN, ints_to_bits
from kuokka.lib_receiver import Receiver, ReceiverConfig, precompile_receiver
from mtools.tools_dsp import waterfall_mx
from mtools.tools_system import mpr_set
from kuokka.lib_tools import make_samples, radionoise
import time, os
from matplotlib import pyplot as plt
from copy import deepcopy


"""
savior_params["sps"] 				= 21		# param ~
savior_params["JPLdecay"] 			= 32.0		# param ~
savior_params["synch_delay_mpr"] 	= 22.0		# param ~
savior_params["lp_cutoff_coeff"] 	= 0.625		# param !
savior_params["lp_ntaps"] 			= 121		# param ~
savior_params["BT"] 				= -1.0		# param !
savior_params["mod_index"] 			= 0.7		# param !
"""

"""
	#============================================
	fftlen			= 1024			# !		(1024,  2048, 512)
	jumplen			= 1024//2		# !		(fftlen / [2,3,4])
	c_stat_update	= 1 / 700		# -		([400:2000])
	c_f_update		= 1.0			# !
	tx_trigger_lvl	= 4.5			# !!!	(4.5 < _ < 9)
	masklen			= int(fftlen/sps)			# !		( int( [0.8:1.2] * fftlen/sps)  )
	mask_c_array	= np.array((1.0, 0.0, 0.0))		# !!	( -1 <= _ <= 1 )
	start_margin_mpr= 1.0			# !!!
	end_margin_mpr  = 0.0			# !
	#============================================
"""



def generate_test_samples(f_tune, f_center, sr0, baudrate, mod_index, BT, n_payloads, noisePpHz, T_init_silence, T_interval_array, T_end_silence):
	assert len(T_interval_array) == (n_payloads-1)
	f_ofst_nrm = (f_center - f_tune) / sr0
	preamble_bits = ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
	rs_mx, rs_cfg = get_default_rs()
	payload_istart_iend_list = list()
	n_init_samples = int(sr0 * T_init_silence)
	n_end_samples = int(sr0 * T_end_silence)
	samples = np.zeros(n_init_samples, dtype=np.complex128)

	for i_pl in range(n_payloads):
		pl = os.urandom(np.random.randint(1,RS_MAX_PL_LEN))
		pl_char_ints = np.array(bytearray(pl), dtype=np.int64)
		bits = frame_packet(pl=pl_char_ints, synchword_int=DEFAULT_SYNCHWORD, synchword_len=DEFAULT_SYNCHWORD_LEN, use_scrambler=True, use_rs=True, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=True)
		bits = np.concatenate( (preamble_bits, bits) )
		tx_sps = sr0 / baudrate
		pl_samples = make_samples(sps_f=tx_sps, bitstring=bits, f_offset=f_ofst_nrm, power=1.0, mod_index=mod_index, shaper_mode=1,
							   shaper_BT_prod=BT, shaper_n_taps=int(tx_sps*4)+1, n_silence_start=0, n_silence_end=0)
		payload_istart_iend_list.append( (pl, len(samples), len(samples)+len(pl_samples)) )
		samples = np.concatenate((samples, pl_samples))
		if i_pl < (n_payloads -1):
			n_interval = int(sr0 * T_interval_array[i_pl])
			samples = np.concatenate((samples, np.zeros(n_interval, dtype=np.complex128)))


	samples = np.concatenate( (samples,np.zeros(n_end_samples, dtype=np.complex128)))
	samples = samples + radionoise(n=len(samples), sr=sr0, W_per_Hz=noisePpHz)
	return samples, payload_istart_iend_list





def feed_samples_to_a_receiver(rx_config, samples, max_batchlen, do_precompile=False):
	if do_precompile:
		precompile_receiver(rx_config=rx_config, do_print=True)
	rx = Receiver(config=rx_config)
	nsamples = len(samples)
	c = 0
	pl_f_cursor_list = list()
	dt_total = 0.0
	while c < len(samples):
		batchlen = min(max_batchlen, nsamples - c)
		batch = samples[c:c+batchlen]
		c += batchlen
		t0 = time.perf_counter()
		ret_list = rx.push_samples(batch, give_bits=False)
		dt_total += (time.perf_counter() - t0)
		for (pl, f_nrm) in ret_list:
			pl_f_cursor_list.append( (pl, float(f_nrm), c) )
	return pl_f_cursor_list, rx.dt_array, dt_total




def tgt_loop(ii, noisePpHz, rx_config, n_payloads, f_tune, f_center_error, rel_baudrate_error, sr0, separate_triggers):
	assert abs(rel_baudrate_error) < 1e-4
	T_init_silence  = 1.25 * (1/rx_config.c_stat_update) * rx_config.jumplen / (rx_config.sps*rx_config.baudrate)
	T_end_silence 	= 5.00 * rx_config.n_delay / (rx_config.sps*rx_config.baudrate)
	T_separate_triggers = 3.0 * (rx_config.start_margin_mpr + rx_config.end_margin_mpr) * rx_config.fftlen / (rx_config.sps*rx_config.baudrate)
	T_interval = 5e-3
	nn = [x for x in ([12,]*int(n_payloads/12) + [n_payloads%12,]) if x>0]
	if separate_triggers:
		T_interval = T_separate_triggers
	n_rcvd = 0
	for n_pl_run in nn:
		samples, payload_istart_iend_list = generate_test_samples(f_tune=f_tune, f_center=rx_config.f_center+f_center_error, sr0=sr0,
																  baudrate=rx_config.baudrate*(1+rel_baudrate_error), mod_index=rx_config.mod_index,
																  BT=rx_config.BT, n_payloads=n_pl_run, noisePpHz=noisePpHz, T_init_silence=T_init_silence,
																  T_interval_array=(T_interval,)*(n_pl_run-1), T_end_silence=T_end_silence)
		pl_f_cursor_list, dt_array, dt_total = feed_samples_to_a_receiver(rx_config=rx_config, samples=samples, max_batchlen=512*2, do_precompile=False)
		n_rcvd += len(pl_f_cursor_list)
	reception_rate = n_rcvd / n_payloads
	return ii, reception_rate

def measure_curve_mpr(rx_config:ReceiverConfig, n_payloads, f_tune, f_center_error, rel_baudrate_error, sr0, noiseP_array, separate_triggers=False):
	reception_rate_array = np.zeros(len(noiseP_array), dtype=np.float64) -1
	argtuples = list()
	for i_noise, noisePpHz in enumerate(noiseP_array):
		argtuples.append( (i_noise, noisePpHz, rx_config, n_payloads, f_tune, f_center_error, rel_baudrate_error, sr0, separate_triggers) )
	ret_list, _ = mpr_set(f=tgt_loop, argtuple_list=argtuples, ncores=7, Q_or_NS="NS", picklepack=True, verbose=False)
	for i_noise, r_rate in ret_list:
		reception_rate_array[i_noise] = r_rate
	assert np.all(reception_rate_array >= 0)
	return reception_rate_array












def get_dealys(payload_istart_iend_list, pl_f_cursor_list, sr0):
	pl_f_cursor_d = dict( [(x[0],x[1:3]) for x in pl_f_cursor_list] )
	delays = np.zeros(len(payload_istart_iend_list))
	for i_pl,(pl,i0,i1) in enumerate(payload_istart_iend_list):
		if pl in pl_f_cursor_d:
			lag_samples = pl_f_cursor_d[pl][1] - i1
			assert lag_samples > 0
			delays[i_pl] = lag_samples
	return delays, delays * 1.0 / sr0



def speed_printout(dt_array, dt_total, nsamples, sr0):
	speed = nsamples / dt_total
	overmatch = speed / sr0
	budget_fraction	= (1/overmatch) / 0.5
	cpu_fraction	= (1/overmatch) / 1.0
	print("="*50)
	print("\tspeed:          {} Ms/s".format( round(1e-6 * speed, 2) ))
	print("\tovermatch:      {}".format( round(overmatch, 2) ))
	print("\tbudget use:     {} %".format( round( 100*budget_fraction , 2) ))
	print("\tcpu core use:   {} %".format( round( 100*cpu_fraction , 2) ))
	print("\t\tpart 1:            {} %".format( round( 100*dt_array[0]/np.sum(dt_array) , 2) ))
	print("\t\tpart 2:            {} %".format( round( 100*dt_array[1]/np.sum(dt_array) , 2) ))
	print("\t\tpart 3:            {} %".format( round( 100*dt_array[2]/np.sum(dt_array) , 2) ))
	print("\t\tpart 4:            {} %".format( round( 100*dt_array[3]/np.sum(dt_array) , 2) ))
	print("="*50)


def random_receiver_config_from_choises(attrname_array_d:dict, rx_config_basis:ReceiverConfig):
	rx_config = deepcopy(rx_config_basis)
	for attrname in attrname_array_d.keys():
		val = attrname_array_d[attrname][np.random.randint(len(attrname_array_d[attrname]))]
		assert hasattr(rx_config, attrname)
		setattr(rx_config, attrname, val)
	return rx_config
# ============================================================================================================================================================================================
# ============================================================================================================================================================================================
# ============================================================================================================================================================================================




def basic_test_A():
	f_tune 		= 437.1e6
	f_center 	= 437.125e6
	sr0 		= 1e6
	baudrate	= 9600
	n_payloads	= 12
	rx_config = ReceiverConfig(sr0=sr0, baudrate=baudrate, bufferlen=800000, batch_maxlen=1024*16, f_tune=f_tune, f_center=f_center)
	rx_config.mod_index = 0.7
	rx_config.BT = -1

	print("[Generating samples]")
	samples, payload_istart_iend_list = generate_test_samples(f_tune=f_tune, f_center=f_center+3.1e3, sr0=sr0,
															  baudrate=baudrate*(1+1.5e-5), mod_index=rx_config.mod_index, BT=rx_config.BT,
															  n_payloads=n_payloads, noisePpHz=0.16/baudrate, T_init_silence=2.0,
															  T_interval_array=(5e-3,)*(n_payloads-1), T_end_silence=0.5)
	print("[Feeding samples]")
	pl_f_cursor_list, dt_array, dt_total = feed_samples_to_a_receiver(rx_config=rx_config, samples=samples, max_batchlen=512*2, do_precompile=True)
	#pl_f_cursor_d = dict( [(x[0],x[1:3]) for x in pl_f_cursor_list] )

	print("\n\n")
	print("Received {}/{} payloads.".format(len(pl_f_cursor_list), n_payloads))
	speed_printout(dt_array=dt_array, dt_total=dt_total, nsamples=len(samples), sr0=sr0)

	delays_s, delays_t = get_dealys(payload_istart_iend_list=payload_istart_iend_list, pl_f_cursor_list=pl_f_cursor_list, sr0=sr0)
	for i_pl,_ in enumerate(payload_istart_iend_list):
		if delays_s[i_pl] > 0:
			print("pl #{}:  lags {} ms.  ({} samples)".format(i_pl, round(1e3*delays_t[i_pl],1), delays_s[i_pl]))
		else:
			print("pl #{}:  missing".format(i_pl))



def measure_curve_for_default_config():
	f_tune 		= 437.1e6
	f_center 	= 437.125e6
	sr0 		= 1e6
	baudrate	= 9600
	n_payloads	= 32
	rx_config1 = ReceiverConfig(sr0=sr0, baudrate=9600, bufferlen=800000, batch_maxlen=1024*16, f_tune=f_tune, f_center=f_center)
	rx_config2 = ReceiverConfig(sr0=sr0, baudrate=9600, bufferlen=800000, batch_maxlen=1024*16, f_tune=f_tune, f_center=f_center)
	rx_config3 = ReceiverConfig(sr0=sr0, baudrate=4800, bufferlen=800000, batch_maxlen=1024*16, f_tune=f_tune, f_center=f_center)
	rx_config2.mod_index = 0.7
	rx_config2.BT = -1

	rel_noiseP_array = np.array([1e-5, 0.02, 0.04, 0.06, 0.08, 0.10, 0.12, 0.14, 0.16, 0.18, 0.19, 0.20, 0.21, 0.22, 0.23, 0.24, 0.26]) # , 0.28
	noiseP_array1 = rel_noiseP_array / 9600
	noiseP_array2 = rel_noiseP_array / 9600
	noiseP_array3 = rel_noiseP_array / 9600

	print("1/6")
	reception_rate_array1 = measure_curve_mpr(rx_config=rx_config1, n_payloads=n_payloads, f_tune=f_tune, f_center_error=3e3, rel_baudrate_error=1.5e-5, sr0=sr0, noiseP_array=noiseP_array1)
	print("2/6")
	reception_rate_array1_sep = measure_curve_mpr(rx_config=rx_config1, n_payloads=n_payloads, f_tune=f_tune, f_center_error=3e3, rel_baudrate_error=1.5e-5, sr0=sr0, noiseP_array=noiseP_array1, separate_triggers=True)
	print("3/6")
	reception_rate_array2 = measure_curve_mpr(rx_config=rx_config2, n_payloads=n_payloads, f_tune=f_tune, f_center_error=3e3, rel_baudrate_error=1.5e-5, sr0=sr0, noiseP_array=noiseP_array2)
	print("4/6")
	reception_rate_array2_sep = measure_curve_mpr(rx_config=rx_config2, n_payloads=n_payloads, f_tune=f_tune, f_center_error=3e3, rel_baudrate_error=1.5e-5, sr0=sr0, noiseP_array=noiseP_array2, separate_triggers=True)
	print("5/6")
	reception_rate_array3 = measure_curve_mpr(rx_config=rx_config3, n_payloads=n_payloads, f_tune=f_tune, f_center_error=3e3, rel_baudrate_error=1.5e-5, sr0=sr0, noiseP_array=noiseP_array3)
	print("6/6")
	reception_rate_array3_sep = measure_curve_mpr(rx_config=rx_config3, n_payloads=n_payloads, f_tune=f_tune, f_center_error=3e3, rel_baudrate_error=1.5e-5, sr0=sr0, noiseP_array=noiseP_array3, separate_triggers=True)


	fig = plt.figure(figsize=(15,11))
	ax1 = fig.add_subplot(111)
	ax1.set_title("Reception rate")

	ax1.plot(rel_noiseP_array, reception_rate_array1, label="default", color="blue")
	ax1.plot(rel_noiseP_array, reception_rate_array1_sep, label="default", linestyle="--", color="blue")
	ax1.plot(rel_noiseP_array, reception_rate_array2, label="mod_idx=0.7, BT=-1", color="orange")
	ax1.plot(rel_noiseP_array, reception_rate_array2_sep, label="mod_idx=0.7, BT=-1", linestyle="--", color="orange")
	ax1.plot(rel_noiseP_array, reception_rate_array3, label="default @ 4800", color="red")
	ax1.plot(rel_noiseP_array, reception_rate_array3_sep, label="default @ 4800 (separate)", linestyle="--", color="red")
	ax1.set_xlabel("relative noise power per 1/baudrate")
	#ax1.semilogx()
	ax1.set_ylabel("%")
	ax1.grid()
	ax1.legend()

	fig.set_layout_engine("tight")
	plt.show()








def optimizer_A():
	f_tune 		= 437.1e6
	f_center 	= 437.125e6
	sr0 		= 1e6
	n_payloads	= 12
	mod_index 	= 0.5
	BT 			= 0.5

	samples, payload_istart_iend_list = generate_test_samples(f_tune=f_tune, f_center=f_center+3.1e3, sr0=sr0,
															  baudrate=9600*(1+1.5e-5), mod_index=mod_index, BT=BT,
															  n_payloads=n_payloads, noisePpHz=0.16/9600, T_init_silence=2.0,
															  T_interval_array=(5e-3,)*(n_payloads-1), T_end_silence=0.5)

	rx_config_basis = ReceiverConfig(sr0=sr0, baudrate=9600, bufferlen=800000, batch_maxlen=1024*16, f_tune=f_tune, f_center=f_center)
	rx_config_basis.mod_index = mod_index
	rx_config_basis.BT = BT
	attrname_array_d = {
		"lp_cutoff_coeff" : [float(x) for x in np.linspace(0.5,1.5, 64)*0.630],
		"synch_delay_mpr" : [float(x) for x in np.linspace(5.0,30.0, 64)],
		"n_delay" :         [int(x) for x in np.linspace(2.0,6.0, 64)*1024],
	}

	rx_config = random_receiver_config_from_choises(attrname_array_d=attrname_array_d, rx_config_basis=rx_config_basis)










basic_test_A()

measure_curve_for_default_config()










