import time, os, pickle
import numpy as np
from kuokka.lib_pll_detector import create_pll_detection_statemx, pll_detect_and_freq_determ_one
from mtools.tools_math import random_point_on_sphere, npv3
from mtools.tools_dsp import waterfall_mx, resampler_execute_stream, resampler_execute, create_resampler
from kuokka.lib_tools import make_samples, radionoise
from matplotlib import pyplot as plt
from tst_receive_recording import get_samples2, fpath0, fpath1, fpath8, fpath9, fpath10, fpath7, fpath6, fpath2, fpath3, fpath4, fpath5

from kuokka.lib_fft_detector import create_fft_centering_statemx, fft_detect_and_freq_determ



def compare():
	sps 			= 17
	baudrate 		= 9600
	sr 				= sps*baudrate
	nsamples 		= int(sr * 6.0)

	f_offset_rel 	= 0.09
	noise_power 	= 0.1 / 9600  # ~0.2 is doable with fft.
	mod_idx 		= 0.5
	BT 				= 0.5

	bits = np.random.randint(0,2, 8*120) * 2 -1
	bits[0:32*1] = [-1,1]*16*1
	tx_samples = make_samples(sps_f=sps, bitstring=bits, f_offset=f_offset_rel, power=1.0, mod_index=mod_idx, shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=301, n_silence_start=0, n_silence_end=0)
	noise = radionoise(n=nsamples, sr=sr, W_per_Hz=noise_power)
	samples = noise.copy()
	i_tx_begin 		= int(0.75 * nsamples)
	samples[i_tx_begin:i_tx_begin + len(tx_samples)] += tx_samples

	centerf5 = -122.46e3
	centerf6 = -124.0e3
	centerf7 = -122.5e3
	centerf8 = -122.5e3
	centerf9 = -123.8e3
	samples = get_samples2(fpath=fpath6)
	samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) * (1/1e6) * centerf6 )
	samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) * (1/1e6) * (f_offset_rel*sr) )
	waterfall_mx(samples, fftlen=2048, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=False)

	samples = samples / (np.max(np.abs(samples))*1.00)
	resamples_mx = create_resampler(m_halflen=25, n_banks=64, r_rate= sr/1e6, f_cutoff=0.499*sr/1e6, allow_aliasing=False)
	samples = resampler_execute(samples=samples, statemx=resamples_mx)
	nsamples = len(samples)
	noiseless_samples = samples.copy()
	noise = radionoise(n=nsamples, sr=sr, W_per_Hz=noise_power)
	samples = samples + noise
	noiseless_amp = np.abs(noiseless_samples)
	noiseless_amp = (np.roll(noiseless_amp, -1) + np.roll(noiseless_amp, 1) + noiseless_amp) * (1/3)




	# FFT ==================================================================
	mask_mode = 1
	fftlen = 1024
	masklen = int(0.5 * (1 + 1.5*(mask_mode==1)) * fftlen / sps)*2 + 1
	fft_statemx = create_fft_centering_statemx(fftlen=fftlen, jumplen=fftlen//2, sps=sps, baudrate=baudrate, search_space_triplet=(-1,-1,-1),
											   mod_index=mod_idx, BT=BT, c_stat_update=1/700, n_delay=fftlen*5, T_f_decay=1.5,
											   fft_trigger_on_level=5.5, fft_trigger_off_level=2.0, masklen=masklen, avg0=0.0, var0=1.0,
											   mask_mode=mask_mode, start_margin_mpr=3.0, end_margin_mpr=1.5)

	fft_center_f_arr = np.zeros(nsamples, dtype=np.float64)
	instr_arr = np.zeros((nsamples,3), dtype=np.float64)
	fft_detect_and_freq_determ(sample_arr=samples, isample0=300, nsamples=nsamples-300, center_f_arr=fft_center_f_arr, center_f_head0=300, statemx=fft_statemx, instr_arr=instr_arr)
	# FFT ==================================================================


	# PLL ==================================================================
	c_freq = 1/sps
	c_phase = c_freq**0.5
	xx = 1.813 * c_freq * sr
	pll_statemx = create_pll_detection_statemx(c_freq=c_freq, c_phase=c_phase, fmin=-0.22, fmax=0.22,
										   std_windowlen=200, trig_on_lvl=0.7*xx, trig_off_lvl=xx, cf_avg_len=64,
										   cf_start_margin=0, cf_end_margin=0, cf_c_update=1/126)

	pll_f_arr = np.zeros(nsamples, dtype=np.float64)
	pll_center_f_arr = np.zeros(nsamples, dtype=np.float64)
	pll_detect_and_freq_determ_one(rs_arr=samples, i_rs0=300, n_samples=nsamples-300, pll_f_arr=pll_f_arr, center_f_arr=pll_center_f_arr, statemx=pll_statemx)
	# PLL ==================================================================

	f_up = f_offset_rel + 9600*0.1/sr
	f_down = f_offset_rel - 9600*0.1/sr
	n_in_box = 0
	n_out_of_box = 0
	for i in range(1024,nsamples-1024):
		if noiseless_amp[i] > 0.5:
			if (f_down < fft_center_f_arr[i]) and (fft_center_f_arr[i] < f_up):
				n_in_box += 1
			else:
				n_out_of_box +=1
	print("In/out:  {} / {} ".format(n_in_box, n_out_of_box))
	print("")


	xx_n = np.arange(nsamples)
	xx_t = np.arange(nsamples) * (1/sr)
	fig = plt.figure(figsize=(14,9))
	ax1 = fig.add_subplot(211)
	ax2 = fig.add_subplot(212)

	#ax1.plot(xx_n, pll_f_arr)
	ax1.plot(xx_n[::10], pll_center_f_arr[::10], label="PLL")
	ax1.plot(xx_n[::10], fft_center_f_arr[::10], label="FFT", color="red")

	y_up = f_offset_rel + 9600*0.1/sr
	y_mid = f_offset_rel - 9600*0.0/sr
	y_down = f_offset_rel - 9600*0.1/sr
	i0_ = 0
	i1_ = xx_n[-1]
	ax1.plot([i0_,i1_], [y_up,y_up], linestyle="--", color="black")
	ax1.plot([i0_,i1_], [y_mid,y_mid], linestyle="--", color="black")
	ax1.plot([i0_,i1_], [y_down,y_down], linestyle="--", color="black")

	ax1.plot(xx_n[::10],  noiseless_amp[::10]-0.5, linestyle="--", color="black")

	ax1.legend()
	ax1.grid()


	ax2.plot(xx_n, instr_arr[:,2])
	ax2.grid()

	fig.set_layout_engine("tight")
	plt.show()









def center_acquisition():
	sps = 17
	baudrate = 9600
	sr = sps*baudrate
	nsamples = int(sr * 4.0)
	i_tx_begin = int(0.313 * nsamples)
	f_offset_rel = 0.12
	noise_power = 0.01 / 9600  # ~0.2 is doable with fft.

	bits = np.random.randint(0,2, 8*120) * 2 -1
	bits[0:32*1] = [-1,1]*16*1
	tx_samples = make_samples(sps_f=sps, bitstring=bits, f_offset=f_offset_rel, power=1.0, mod_index=0.5, shaper_mode=1, shaper_BT_prod=0.5, shaper_n_taps=301, n_silence_start=0, n_silence_end=0)
	corr_samples = make_samples(sps_f=sps, bitstring=bits[0:32], f_offset=f_offset_rel*0, power=1.0, mod_index=0.5, shaper_mode=1, shaper_BT_prod=0.5, shaper_n_taps=301, n_silence_start=0, n_silence_end=0)
	noise = radionoise(n=nsamples, sr=sr, W_per_Hz=noise_power)
	samples = noise.copy()
	samples[i_tx_begin:i_tx_begin + len(tx_samples)] += tx_samples

	samples = get_samples2(fpath=fpath6)
	#waterfall_mx(samples=samples, fftlen=1024*2, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=False)
	samples = samples / np.average( np.abs(samples[int(2.5e6):int(2.6e6)]) )


	samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) * (1/1e6) * -100e3 )
	rsmplr = create_resampler(m_halflen=25, n_banks=64, r_rate=(sr/1e6), f_cutoff=0.499*(sr/1e6), allow_aliasing=False)
	samples = resampler_execute(samples=samples, statemx=rsmplr)
	samples = samples + radionoise(n=len(samples), sr=1e6, W_per_Hz=noise_power)
	nsamples = len(samples)


	#samples = samples[0:len(samples)//3]

	#sr = 1e6
	#sps = sr / baudrate


	c_freq = 1/sps
	c_phase = c_freq**0.5
	xx = 1.813 * c_freq * sr
	statemx = create_pll_detection_statemx(c_freq=c_freq, c_phase=c_phase, fmin=-0.22, fmax=0.22,
										   std_windowlen=200, trig_on_lvl=0.7*xx, trig_off_lvl=xx, cf_avg_len=64,
										   cf_start_margin=0, cf_end_margin=0, cf_c_update=1/126)

	pll_f_arr = np.zeros(nsamples, dtype=np.float64)
	center_f_arr = np.zeros(nsamples, dtype=np.float64)
	pll_detect_and_freq_determ_one(rs_arr=samples, i_rs0=300, n_samples=nsamples-300, pll_f_arr=pll_f_arr, center_f_arr=center_f_arr, statemx=statemx)

	corr0 = np.correlate(samples, corr_samples)
	corr = np.abs(corr0)

	waterfall_mx(samples=samples, fftlen=1024*2, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=False)
	#waterfall_mx(samples=corr0, fftlen=1024*2, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=True)


	xx_n = np.arange(nsamples)
	xx_t = np.arange(nsamples) * (1/sr)

	fig = plt.figure(figsize=(14,9))
	ax1 = fig.add_subplot(211)
	ax2 = fig.add_subplot(212)

	ax1.plot(xx_n, pll_f_arr)
	ax1.plot(xx_n, center_f_arr)

	y_up = f_offset_rel + 9600*0.25/sr
	y_down = f_offset_rel - 9600*0.25/sr
	ax1.plot([i_tx_begin,i_tx_begin], [-1,1], linestyle="--", color="black")
	ax1.plot([i_tx_begin+sps*32,i_tx_begin+sps*32], [-1,1], linestyle="--", color="black")
	ax1.plot([i_tx_begin,i_tx_begin+sps*232], [y_up,y_up], linestyle="--", color="black")
	ax1.plot([i_tx_begin,i_tx_begin+sps*232], [y_down,y_down], linestyle="--", color="black")
	ax1.grid()

	ax2.plot(corr)
	ax2.plot([i_tx_begin,i_tx_begin], [0,100], linestyle="--", color="black")
	ax2.plot([i_tx_begin+sps*32,i_tx_begin+sps*32], [0,100], linestyle="--", color="black")
	ax2.grid()

	fig.set_layout_engine("tight")
	plt.show()




#center_acquisition()
compare()









