import numpy as np
from matplotlib import pyplot as plt
import pickle
import time
from mtools.tools_dsp import waterfall_mx, create_resampler, resampler_execute
from kuokka.lib_fft_detector import create_fft_centering_statemx, fft_detect_and_freq_determ
from kuokka.lib_pll_detector import create_frequency_mapper_statev, pll_detect_and_freq_determ
from mtools.tools_dsp import create_pll_statevector, create_trigger_statevector, create_winstd_statemx
from kuokka.lib_tools import radionoise, make_samples, get_frequency_search_map, get_doppler_low_high




def speedbench_fft_detect(sps, baudrate):
	mod_idx = 0.7
	BT = -1
	bits = np.random.randint(0,2,1000)*2 -1
	sr = baudrate * sps
	f_tune = 437e6
	f_signal = f_tune + 0.05*sr
	doppler_max = 10928.125  # 10928.125
	f_offset_rel = (f_signal - f_tune) / sr
	assert abs(f_offset_rel) < 0.25
	sig_samples = make_samples(sps_f=sps, bitstring=bits, f_offset=f_offset_rel, power=1.0, mod_index=mod_idx, shaper_mode=0, shaper_BT_prod=-1, shaper_n_taps=1)
	nsignal = len(sig_samples)
	samples0 = np.zeros(nsignal*3, dtype=np.complex128)
	samples1 = np.concatenate( (np.zeros(nsignal, dtype=np.complex128), sig_samples,  np.zeros(nsignal, dtype=np.complex128)) )
	nsamples = len(samples1)
	assert len(samples0) == len(samples1)
	samples0 = samples0 + radionoise(n=nsamples, sr=sr, W_per_Hz=0.2/baudrate)
	samples1 = samples1 + radionoise(n=nsamples, sr=sr, W_per_Hz=0.2/baudrate)

	fftlen 					= 1024
	c_stat_update 			= 1/700
	n_delay 				= fftlen*5
	fft_trigger_on_level 	= 5.5
	fft_trigger_off_level 	= 2.0
	f_center_min_nrm		= (f_signal-doppler_max*1.2-f_tune)/sr
	f_center_max_nrm		= (f_signal+doppler_max*1.2-f_tune)/sr
	f_center_search_map		= get_frequency_search_map(fftlen=fftlen, f_min_nrm=f_center_min_nrm, f_max_nrm=f_center_max_nrm)
	start_margin_mpr		= 3.0
	end_margin_mpr			= 1.5
	mask_mode				= 1
	fft_statemx = create_fft_centering_statemx(fftlen=fftlen, sps=sps, f_center_search_map=f_center_search_map,
											   mod_index=mod_idx, BT=BT, c_stat_update=c_stat_update, n_delay=n_delay,
											   fft_trigger_on_level=fft_trigger_on_level, fft_trigger_off_level=fft_trigger_off_level,
								 			   avg0=0.0, var0=1.0, mask_mode=mask_mode, start_margin_mpr=start_margin_mpr, end_margin_mpr=end_margin_mpr)

	center_f_arr = np.zeros(nsamples, dtype=np.float64)
	instr_arr = np.zeros((nsamples,3), dtype=np.float64)

	_ = fft_detect_and_freq_determ(sample_arr=samples0, isample0=0, nsamples=100, center_f_arr=center_f_arr, center_f_head0=0, statemx=fft_statemx, instr_arr=instr_arr)
	_ = fft_detect_and_freq_determ(sample_arr=samples0, isample0=0, nsamples=100, center_f_arr=center_f_arr, center_f_head0=0, statemx=fft_statemx, instr_arr=instr_arr)
	_ = fft_detect_and_freq_determ(sample_arr=samples0, isample0=0, nsamples=nsamples, center_f_arr=center_f_arr, center_f_head0=0, statemx=fft_statemx, instr_arr=instr_arr)

	sample_head = 0
	center_f_head = 0
	batchlen = 1000
	t0 = time.perf_counter()
	while sample_head < nsamples:
		batchlen = min(batchlen, nsamples - sample_head)
		center_f_head, demod_head = fft_detect_and_freq_determ(sample_arr=samples0, isample0=sample_head, nsamples=batchlen, center_f_arr=center_f_arr, center_f_head0=center_f_head, statemx=fft_statemx, instr_arr=instr_arr)
		sample_head += batchlen
	T_total = (time.perf_counter() - t0)
	speed = nsamples / T_total
	overmatch = speed / sr
	core_fraction	= (1/overmatch) / 1.0
	budget_fraction	= (1/overmatch) / 0.5
	print("-- FFT-DETECTION -----------------------------------")
	print("sps:{}        baudrate:{}".format(sps, baudrate))
	print("no signal:")
	print("\tspeed:              {} Ms/s".format( round( 1e-6*speed, 3) ))
	print("\tovermatch:          {}".format( round( overmatch, 3) ))
	print("\tcore use:           {} %".format( round( 100*core_fraction, 2) ))
	print("\tbudget use:         {} %".format( round( 100*budget_fraction, 2) ))

	sample_head = 0
	center_f_head = 0
	batchlen = 1000
	t0 = time.perf_counter()
	while sample_head < nsamples:
		batchlen = min(batchlen, nsamples - sample_head)
		center_f_head, demod_head = fft_detect_and_freq_determ(sample_arr=samples1, isample0=sample_head, nsamples=batchlen, center_f_arr=center_f_arr, center_f_head0=center_f_head, statemx=fft_statemx, instr_arr=instr_arr)
		sample_head += batchlen
	T_total = (time.perf_counter() - t0)
	speed = nsamples / T_total
	overmatch = speed / sr
	core_fraction	= (1/overmatch) / 1.0
	budget_fraction	= (1/overmatch) / 0.5
	print("With signal:")
	print("\tspeed:              {} Ms/s".format( round( 1e-6*speed, 3) ))
	print("\tovermatch:          {}".format( round( overmatch, 3) ))
	print("\tcore use:           {} %".format( round( 100*core_fraction, 2) ))
	print("\tbudget use:         {} %".format( round( 100*budget_fraction, 2) ))
	print("----------------------------------------------------")
	print("")










def tst_fft_center_detect_1():
	f_tune 			= 437.1e6
	f_signal 		= 437.125e6
	f_offset 		= f_signal - f_tune
	doppler_max 	= 10928.125  # 10928.125
	sps				= 21
	baudrate		= 9600
	sr 				= sps*baudrate
	nnoise_init		= sps * 1400 * 14
	nnoise_mid		= sps * 200
	nnoise_end		= sps * 1200
	BT = -1
	f_offset_rel1 	= f_offset / sr
	f_offset_rel2 	= f_offset / sr
	f_offset_rel3 	= f_offset / sr

	f_offset_rel	= 0.0
	mod_index 		= 0.707
	bits 			= np.random.randint(0,2, 600)*2-1
	sig_samples1 	= make_samples(sps_f=sps, bitstring=bits, f_offset=f_offset_rel1, power=1.0, mod_index=mod_index, shaper_mode=0, shaper_BT_prod=-1, shaper_n_taps=sps*6+1)
	sig_samples2 	= make_samples(sps_f=sps, bitstring=bits, f_offset=f_offset_rel2, power=1.0, mod_index=mod_index, shaper_mode=0, shaper_BT_prod=-1, shaper_n_taps=sps*6+1)
	sig_samples3 	= make_samples(sps_f=sps, bitstring=bits, f_offset=f_offset_rel3, power=1.0, mod_index=mod_index, shaper_mode=0, shaper_BT_prod=-1, shaper_n_taps=sps*6+1)
	nsignal			= len(sig_samples1)
	noise1 			= np.zeros(nnoise_init, dtype=np.complex128)
	noise2 			= np.zeros(nnoise_mid, dtype=np.complex128)
	noise3 			= np.zeros(nnoise_end, dtype=np.complex128)
	samples0 		= np.concatenate( (noise1, sig_samples1, noise2, sig_samples2, noise2, sig_samples3, noise3) )
	samples0		= samples0
	nsamples 		= len(samples0)
	samples 		= samples0.copy()
	sig_x_arr 		= np.zeros( (3,2) )
	sig_x_arr[0]	= np.array( (nnoise_init, nnoise_init+nsignal))
	sig_x_arr[1]	= np.array( (nnoise_init+nsignal+nnoise_mid, nnoise_init+nsignal*2+nnoise_mid))
	sig_x_arr[2]	= np.array( (nnoise_init+nsignal*2+nnoise_mid*2, nnoise_init+nsignal*3+nnoise_mid*2))
	sig_y_arr		= np.zeros( (3,2) )
	sig_y_arr[0]	= np.array( (f_offset_rel1, f_offset_rel1) )
	sig_y_arr[1]	= np.array( (f_offset_rel2, f_offset_rel2) )
	sig_y_arr[2]	= np.array( (f_offset_rel3, f_offset_rel3) )
	samples 		= samples + radionoise(n=len(samples), sr=sr, W_per_Hz=0.050/9600)


	curtain_arr = np.abs( samples )
	curtain_arr = curtain_arr / np.max(curtain_arr)
	#noiseamp = 1.6
	#samples = samples + np.random.normal(0, noiseamp, nsamples) + 1j*np.random.normal(0,noiseamp, nsamples)



	waterfall_mx(samples=samples, fftlen=2*sps//2, fft_jump=2*sps//4, srate=sr, plot_and_show=True, y_is_time=False)
	waterfall_mx(samples=samples, fftlen=1024*2, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=False)

	# FFT DETECTION =======================================================================================================================================
	#============================================
	fftlen				= 1024
	c_stat_update		= 1 / 700
	n_delay				= fftlen*5
	trigger_on_lvl		= 5.5
	trigger_off_lvl		= 0.0
	mask_mode			= 1
	start_margin_mpr	= 3.0
	end_margin_mpr  	= 1.5
	#============================================
	#f_tune = 437e6
	#f_signal = 437.125e6
	#doppler_max = 10928.125  # 10928.125
	f_center_min_nrm 	= (f_signal-f_tune-doppler_max*1.2) / sr
	f_center_max_nrm 	= (f_signal-f_tune+doppler_max*1.2) / sr
	f_center_search_map	= get_frequency_search_map(fftlen=fftlen, f_min_nrm=f_center_min_nrm, f_max_nrm=f_center_max_nrm)
	#f_center_search_map = np.ones(fftlen)*1.0

	center_f_arr_fft0 	= np.zeros(nsamples, dtype=np.float64) -1
	center_f_arr_fft1 	= np.zeros(nsamples, dtype=np.float64) -1
	instr_arr 			= np.zeros((nsamples,3), dtype=np.float64) -1

	print("Creating statemx.")
	statemx0 = create_fft_centering_statemx(fftlen=fftlen, sps=sps, f_center_search_map=f_center_search_map,
											mod_index=mod_index, BT=BT, c_stat_update=c_stat_update, n_delay=n_delay,
											fft_trigger_on_level=trigger_on_lvl, fft_trigger_off_level=trigger_off_lvl,
											avg0=0.0, var0=1.0, mask_mode=mask_mode, start_margin_mpr=start_margin_mpr, end_margin_mpr=end_margin_mpr)

	print("Runnign fft centering and detection in one go")
	fft_detect_and_freq_determ(sample_arr=samples, isample0=0, nsamples=nsamples, center_f_arr=center_f_arr_fft0, center_f_head0=0, statemx=statemx0.copy(), instr_arr=instr_arr)

	print("Runnign fft centering and detection in bathces")
	feed_head = 0
	statemx2 = statemx0.copy()
	while feed_head < nsamples:
		nbatch = np.random.randint(0, fftlen*2+2)
		if nbatch == 0:
			print("NBATCH=0 at",feed_head)
		#nbatch = fftlen +3
		nbatch = min(nbatch, nsamples - feed_head)
		fft_detect_and_freq_determ(sample_arr=samples, isample0=feed_head, nsamples=nbatch, center_f_arr=center_f_arr_fft1, center_f_head0=feed_head, statemx=statemx2, instr_arr=instr_arr)
		feed_head += nbatch

	diffs = np.abs((np.isclose(center_f_arr_fft0, center_f_arr_fft1) * 1.0)-1.0)
	differing_indexes = np.array([i for i in range(len(diffs)) if diffs[i]])

	print("outputs differ in {} points".format(sum(diffs)))
	print("such as:      ", differing_indexes[0:10])
	print("which are at: ", differing_indexes[0:10]/len(diffs))

	plt.plot(center_f_arr_fft0 - center_f_arr_fft1)
	plt.grid()
	plt.show()

	assert np.allclose(center_f_arr_fft0, center_f_arr_fft1)

	print("Outputs of batched run and one-go run match.")

	trigger_arr_fft = center_f_arr_fft1 > -0.5
	avg_arr = instr_arr[:,0]
	var_arr = instr_arr[:,1]
	fftmax_arr = instr_arr[:,2]
	# FFT DETECTION =======================================================================================================================================


	# PLL DETECTION =======================================================================================================================================
	#============================================
	Z0 				= 1.81 * 0.001 * 1.0	# !!!
	c_freq 			= 0.001					# !!!
	c_phase			= c_freq**0.5			# ?
	f_limit			= 0.3					# -
	windowlen		= 300					# !
	trig_on_lvl		= Z0 * 0.7				# !!!
	trig_off_lvl	= Z0					# -
	avg_len			= sps * 8 * 4			# !
	start_margin	= sps * 8 * 3			# -
	end_margin		= sps * 8 * 3			# -
	f_upd_divisor	= 400					# -
	#============================================
	pllstatev 			= create_pll_statevector(srate=1.0, c_freq=c_freq, c_phase=c_phase, fmin=-f_limit, fmax=f_limit)
	winstd_statemx 		= create_winstd_statemx(windowlength=windowlen, avg0=0.0, std0=1.0)
	trig_statev 		= create_trigger_statevector(on_lvl=trig_on_lvl, off_lvl=trig_off_lvl, state0=0)
	cfmap_statev 		= create_frequency_mapper_statev(avg_len=avg_len, start_margin=start_margin, end_margin=end_margin, cc_update_divisor=f_upd_divisor)
	pll_f_arr 			= np.zeros(nsamples, dtype=np.float64)
	df_std_arr 			= np.zeros(nsamples, dtype=np.float64)
	trig_arr 			= np.zeros(nsamples, dtype=np.int64)
	center_f_arr_pll 	= np.zeros(nsamples, dtype=np.float64) -1

	pll_detect_and_freq_determ(rs_arr=samples, i_rs0=0, n_samples=nsamples,						pll_f_arr=pll_f_arr, pllstatev=pllstatev.copy(),
							   df_std_arr=df_std_arr, winstd_statemx=winstd_statemx.copy(),		trig_arr=trig_arr, trig_statev=trig_statev.copy(),
							   center_f_arr=center_f_arr_pll, cfmapstatev=cfmap_statev.copy())
	trigger_arr_pll = center_f_arr_pll > -0.5
	# PLL DETECTION =======================================================================================================================================





	# PLOTTING ============================================================================================================================================
	waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, srate=1.0, plot_and_show=True, y_is_time=False)

	fig = plt.figure(figsize=(15,13))
	ax1 = fig.add_subplot(311)
	ax2 = fig.add_subplot(312)
	ax3 = fig.add_subplot(313)

	ax1.plot( np.arange(nsamples), (center_f_arr_fft1-f_offset_rel)*sr )
	ax1.plot( np.arange(nsamples), (center_f_arr_pll-f_offset_rel)*sr )
	for isig in range(len(sig_x_arr)):
		ax1.plot( sig_x_arr[isig], (sig_y_arr[isig]-f_offset_rel)*sr , linestyle="--", marker="x")
	ax1.grid()

	ax2.plot( np.arange(nsamples), trigger_arr_fft )
	ax2.plot( np.arange(nsamples), trigger_arr_pll )
	ax2.plot( np.arange(nsamples), fftmax_arr )
	ax2.plot( np.arange(nsamples), curtain_arr, color="black" )
	ax2.grid()

	ax3.plot( np.arange(nsamples), avg_arr )
	ax3.plot( np.arange(nsamples), var_arr )
	ax3.grid()

	fig.set_layout_engine("tight")
	plt.show()
	# PLOTTING ============================================================================================================================================









speedbench_fft_detect(sps=20, baudrate=9600)
speedbench_fft_detect(sps=20, baudrate=9600*2)
speedbench_fft_detect(sps=20, baudrate=9600*4)


tst_fft_center_detect_1()





