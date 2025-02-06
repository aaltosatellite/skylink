import numpy as np
import pickle
from matplotlib import pyplot as plt
import time
#zz = samples * np.conj(np.roll(samples, -1))
#fmdemod = np.arctan2(zz.imag, zz.real)[:-2]
#fmdemod = fmdemod * srate/(2*np.pi)
from scipy.signal import firwin
from mtools.tools_dsp import waterfall_mx, fft_usual, winstd_cont, winstd_init, trigger_vector_cont, triggered_average_cont, expdec_std
from mtools.tools_dsp import pll_cont, pll_single_shot, pll_cont_strm, create_pll_statevector
from kuokka.lib_tools import make_samples





def tst_visualize():
	srate = 1e6
	fftlen = 2048
	fft_jump = 1000

	fpath = "/home/elmore/datasetit/radiotallenteet/uhf-nayte-{}.dat".format(96)
	f = open(fpath, "rb")
	samples = pickle.loads(f.read())
	f.close()
	print("Samples type:   {}".format(type(samples)))
	print("Samples length: {}".format(len(samples)))
	print("Samples dtype:  {}".format(samples.dtype))


	# CUT
	samples = samples[4150000:4400000]
	#samples = samples[4150000:4250000]

	# Frequency shift
	samples = samples * np.exp((2j*np.pi/srate) * (0*-126e3*np.arange(len(samples))))

	# FFT bechmark
	t0 = time.perf_counter()
	for _ in range(60):
		_ = np.fft.fft(samples[0:2048])
	dt = (time.perf_counter() - t0)/60
	print("fft2048:  {} µs".format(  round(dt*1e6, 1)  ))

	# Waterfall
	mx, extent, aspect = waterfall_mx(samples=samples, fftlen=fftlen, fft_jump=fft_jump, srate=srate)
	fig = plt.figure(figsize=(14,14))
	ax = fig.add_subplot(111)
	ax.imshow(mx, origin="lower",  extent=extent, aspect=aspect)
	fig.set_layout_engine("tight")
	plt.show()

	"""samples = samples[::10]
	nsamples = len(samples)
	yy = np.linspace(0, nsamples-1, nsamples)
	fig = plt.figure(figsize=(14,14))
	ax = fig.add_subplot(111, projection="3d")
	ax.scatter(samples.real, yy, samples.imag)
	fig.set_layout_engine("tight")
	plt.show()"""
	#naive_pll(samples, srate=srate)






def tst_exp_decay_lowpass():
	noise_a = np.random.normal(0, 1, 80000) + 1j * np.random.normal(0, 1, 80000)
	noise_b = noise_a * 0.0
	for i in range(1, len(noise_a)):
		noise_b[i] = noise_b[i-1]*0.8 + noise_a[i]*(1-0.8)
	fft_a, freqs_a = fft_usual(iq_arr=noise_a, srate=1e6, take_abs=True)
	fft_b, freqs_b = fft_usual(iq_arr=noise_b, srate=1e6, take_abs=True)
	plt.plot(freqs_a, fft_a)
	plt.plot(freqs_b, fft_b)
	plt.grid()
	plt.title("exp decay Lowpass")
	plt.show()









def naive_pll_tst(smode):
	assert smode in ("A", "B")
	dash_idxs = list()

	if smode == "A":
		sr 			= 1e6
		symbolrate 	= 9600
		f_offset	= 1e4
		bits 		= np.random.randint(0, 2, 360) * 2  -1
		samples1 	= make_samples(sps_f=sr/symbolrate, bitstring=bits, f_offset=f_offset/sr, power=1, sr_for_power=1.0, mod_index=0.5, shaper_mode=1, shaper_BT_prod=0.5, shaper_n_taps=301)
		nsignal 	= len(samples1)
		print("{} samples".format( len(samples1) ))
		print("max sample magnitude: ", np.max(np.abs(samples1)) )
		print("avg sample magnitude: ", np.average(np.abs(samples1)) )
		nnoise = int(nsignal*0.2)
		noise1 		= np.random.normal(0, 0.798,  nnoise) + 1j*np.random.normal(0, 0.7981,  nnoise)
		noise2 		= np.random.normal(0, 0.798,  nnoise) + 1j*np.random.normal(0, 0.7981,  nnoise)
		samples1 	= np.concatenate( (noise1, samples1, noise2) )
		dash_idxs.append(nnoise)
		dash_idxs.append(nnoise + nsignal)


	else:
		fpath = "/home/elmore/datasetit/radiotallenteet/uhf-nayte-{}.dat".format(96)
		f = open(fpath, "rb")
		samples1 = pickle.loads(f.read())
		sr = 1e6
		symbolrate = 9600
		f_offset0 = 1.2442e5
		f_offset = 15.0e3
		f.close()
		assert type(samples1) == np.ndarray, type(samples1)
		assert samples1.dtype in (np.complex128, np.complex64), samples1.dtype
		# CUT
		samples1 = samples1[4150000:4400000]
		samples1 = samples1 * np.exp(2j*np.pi * (1/sr) * (f_offset-f_offset0)*np.arange(len(samples1)))
		avgamp = np.average( np.abs(samples1) )
		print("\tavg amplitude: ",avgamp)
		samples1 = samples1 * (1/avgamp)
		avgamp = np.average( np.abs(samples1) )
		print("\tavg amplitude: ",avgamp)

	print("Samplecount: {}".format(len(samples1)))
	print("Total Time:  {} ms".format(round(1000.0*len(samples1)/sr, 3)))

	NOISE1 = 0.5
	NOISE2 = 0.7
	T_sample = 1/sr
	nsamples 	= len(samples1)
	samples2 	= samples1.copy()
	samples1 	= samples1 + np.random.normal(0, NOISE1*0.79813, nsamples) + 1j*np.random.normal(0, NOISE1*0.79813, nsamples)
	samples1 	= samples1 / np.average(np.abs(samples1))
	samples2 	= samples2 + np.random.normal(0, NOISE2*0.79813, nsamples) + 1j*np.random.normal(0, NOISE2*0.79813, nsamples)
	samples2 	= samples2 / np.average(np.abs(samples2))

	n_lowpass_taps = 201 # TODO: Optimize...
	lowpass_cutoff_coeff = 0.6366 * 1.2 # TODO: Optimize...  (0.62 best?) NOTE: Use correlation (or similarity) between FMdemod of noiseless and noised transmission as a criterion.
	lowpass_cutoff = symbolrate * lowpass_cutoff_coeff
	# noinspection PyTypeChecker
	lowpass_taps = firwin(n_lowpass_taps, cutoff=lowpass_cutoff, fs=sr, pass_zero=True)



	# WATERFALL
	mx, extent, aspect = waterfall_mx(samples=samples1, fftlen=2048, fft_jump=1000, srate=sr, plot_and_show=False, y_is_time=False)
	fig = plt.figure(figsize=(14,14))
	ax = fig.add_subplot(111)
	ax.imshow(mx, origin="lower",  extent=extent, aspect=aspect)
	fig.set_layout_engine("tight")
	plt.show()



	# FFT SERIES
	n_fft = 36
	fft_skip = int((nsamples-2048) / n_fft)
	print("\tfft skp: ",fft_skip)
	fig = plt.figure(figsize=(14,14))
	ax = fig.add_subplot(111)
	fftavg = np.zeros(2048)
	fftfreq = np.arange(2048)
	fft_list = list()
	t00 = time.perf_counter()
	for ifft in range(n_fft):
		fft, fftfreq = fft_usual(iq_arr=samples2[ifft*fft_skip:ifft*fft_skip+2048], srate=sr, take_abs=True)
		colorcode = ("00"+hex( int(ifft * 230/(n_fft+1)) )[2:])[-2:]
		colorcode = "#" + colorcode*3
		fftavg += fft
		fft_list.append( (fft,colorcode) )
	dtfft = time.perf_counter() - t00
	print("dt-fft:             {} ms".format( round(1000*dtfft, 3 ) ))
	for fft, colorcode in fft_list:
		ax.plot(fftfreq, fft, color=colorcode)
	ax.plot(fftfreq, fftavg*1/n_fft, color="red")
	ax.grid()
	fig.set_layout_engine("tight")
	plt.show()



	# PLL LOOPS
	c_freq = 0.001 * 1.0		# TODO: Optimize... (0.001 good for 1MSps)
	c_phase = c_freq**0.5
	pllstatev = create_pll_statevector(srate=sr, c_freq=c_freq, c_phase=c_phase, fmin=-35e3, fmax=35e3)
	_ = pll_cont(samples=samples1[0:100], statev=pllstatev)
	_ = pll_cont(samples=samples1[0:100], statev=pllstatev)
	t00 = time.perf_counter()
	pll_f_arr1 = pll_cont(samples=samples1, statev=pllstatev)  # c_phase=0.1, c_freq=0.001, c_sample_rem=0.8 THIS SEEMS DECENT FOR NOISE LVL 0.6
	dt_pll = time.perf_counter() - t00
	pll_f_arr2 = pll_cont(samples=samples2, statev=pllstatev)
	t_arr = np.arange(nsamples)*T_sample*1000.0
	x_arr = np.arange(nsamples)
	fdelta_1 = pll_f_arr1 - np.roll(pll_f_arr1, 1)
	fdelta_2 = pll_f_arr2 - np.roll(pll_f_arr2, 1)
	fdelta_1[0] = 0
	fdelta_2[0] = 0
	print("dt-pll:             {} ms".format( round(dt_pll*1000, 3) ))



	# LOWPASS
	dt_lp = 0.0
	dt_demod = 0.0
	dmdd = list()
	for smpls, do_lp in ((samples1, True), (samples2, True)):
		centered = smpls * np.exp(2j*np.pi * (1/sr) * -(f_offset+0.0e3)*np.arange(nsamples))
		if do_lp:
			t00 = time.perf_counter()
			lowpassed = np.convolve(centered, lowpass_taps, mode="valid")
			if dt_lp == 0.0:
				dt_lp += (time.perf_counter() - t00)
		else:
			i0 = len(lowpass_taps)//2
			i1 = len(centered) - (len(lowpass_taps) - i0) +1
			lowpassed = centered[i0:i1]
		t00 = time.perf_counter()
		lowpassed_zz = lowpassed * np.conj(np.roll(lowpassed, 1))
		lowpassed_dmd = np.arctan2(lowpassed_zz.imag, lowpassed_zz.real)
		if dt_demod == 0.0:
			dt_demod += (time.perf_counter() - t00)
		dmdd.append(lowpassed_dmd)
	S1_lp_dmd, S2_lp_dmd = dmdd
	assert len(S1_lp_dmd) == len(S2_lp_dmd)
	print("dt-lp:              {} ms".format( round(dt_lp*1000, 3) ))
	print("dt-demod:           {} ms".format( round(dt_demod*1000, 3) ))



	# WINDOWED STANDARD DEVIATION
	STD_windowlen = 200 # TODO: Optimize...
	_, _, _ = expdec_std(arr=np.random.random(100), N=400, avg0=0.5, var0=0.71)
	_, _a, _b = winstd_init(arr=np.random.random(100), windowlen=30)
	_, _, _ = winstd_cont(arr=np.random.random(100), avg_contr_arr=_a, var_contr_arr=_b)
	t00 = time.perf_counter()
	fdelta_1_winstd_classic = np.array([np.std(fdelta_1[max(0, i-STD_windowlen):max(i,STD_windowlen)]**1)**1 for i in range(len(fdelta_1))])
	dt_winstd_oneline, t00 = time.perf_counter()-t00, time.perf_counter()
	fdelta_1_winstd_expdec, _, _ = expdec_std(arr=fdelta_1, N=STD_windowlen//2, avg0=np.average(fdelta_1[0:STD_windowlen//4]), var0=np.std(fdelta_1[0:STD_windowlen//4])**2)
	dt_winstd_expdec, t00 = time.perf_counter()-t00,  time.perf_counter()
	fdelta_1_winstd_pseudo, _, _ = winstd_init(arr=fdelta_1, windowlen=STD_windowlen)
	dt_winstd_pseudo = time.perf_counter()-t00
	f2_delta_std_window, _, _ = winstd_init(arr=fdelta_2, windowlen=STD_windowlen)
	#print("std0 1: ",fdelta_1_winstd_classic[0])	# A: 1779.4  B: 1819.77
	#print("std0 2: ",f2_delta_std_window[0])	# A: 1747.8  B: 1736.74
	print("dt-winstd-oneline:  {} ms".format( round(dt_winstd_oneline*1000, 3) ))
	print("dt-winstd-expdec:   {} ms".format( round(dt_winstd_expdec*1000, 3) ))
	print("dt-winstd-pseudo:   {} ms".format( round(dt_winstd_pseudo*1000, 3) ))


	std00_cap = 1.813 * c_freq * sr
	print("STD cap: {}".format(std00_cap))
	fig = plt.figure(figsize=(14,14))
	ax1 = fig.add_subplot(211)
	ax1.plot(np.arange(len(fdelta_1_winstd_classic)), fdelta_1_winstd_classic/std00_cap , label="windowed std")
	ax1.plot(np.arange(len(fdelta_1_winstd_expdec)), fdelta_1_winstd_expdec/std00_cap , label="expdec std")
	ax1.plot(np.arange(len(fdelta_1_winstd_pseudo)), fdelta_1_winstd_pseudo/std00_cap , label="pseudo-window std")
	ax1.grid()
	ax1.legend()

	ax2 = fig.add_subplot(212)
	ax2.plot(np.arange(len(fdelta_1_winstd_classic)), std00_cap/fdelta_1_winstd_classic , label="windowed std")
	ax2.plot(np.arange(len(fdelta_1_winstd_expdec)), std00_cap/fdelta_1_winstd_expdec , label="expdec std")
	ax2.plot(np.arange(len(fdelta_1_winstd_pseudo)), std00_cap/fdelta_1_winstd_pseudo , label="pseudo-window std")
	ax2.grid()
	ax2.legend()
	fig.set_layout_engine("tight")
	plt.show()



	# Frequeny averaging by window STD trigger
	_ = trigger_vector_cont(criterion_arr=fdelta_1_winstd_pseudo, trig_on_lvl=1200, trig_off_lvl=1550, state0=False)
	_ = trigger_vector_cont(criterion_arr=fdelta_1_winstd_pseudo, trig_on_lvl=1200, trig_off_lvl=1550, state0=False)
	t00 = time.perf_counter()
	trig_arr, _ = trigger_vector_cont(criterion_arr=fdelta_1_winstd_pseudo, trig_on_lvl=std00_cap*0.7, trig_off_lvl=std00_cap*1.0, state0=False)
	dt_trigg = time.perf_counter() - t00
	print("dt-trigg-only:      {} ms".format( round(dt_trigg*1000, 3) ))

	_ = triggered_average_cont(arr=pll_f_arr1, trig_arr=trig_arr, divcount0=0, avg_buffer0=0)
	_ = triggered_average_cont(arr=pll_f_arr1, trig_arr=trig_arr, divcount0=0, avg_buffer0=0)
	t00 = time.perf_counter()
	f1_avg_series, _, _ = triggered_average_cont(arr=pll_f_arr1, trig_arr=trig_arr, divcount0=0, avg_buffer0=0)
	dt_avg_by_t = time.perf_counter() - t00
	print("dt-avg-by-trigvec:  {} ms".format( round(dt_avg_by_t*1000, 3) ))


	fig = plt.figure(figsize=(14,14))
	ax1 = fig.add_subplot(311)
	ax2 = fig.add_subplot(312)
	ax3 = fig.add_subplot(313)

	ax1.set_title("0-Phase term")
	#ax1.plot(t_arr, pll_p0_arr1, label="p1")
	#ax1.plot(t_arr, pll_p0_arr2, label="p2")
	ax1.plot(t_arr[0:len(S1_lp_dmd)], S1_lp_dmd, label="Demod-1")
	#ax1.plot(t_arr[0:len(S2_lp_dmd)], S2_lp_dmd, label="Demod-2")
	ax1.grid()
	ax1.legend()

	ax2.set_title("frequency term")
	for dash_idx in dash_idxs:
		ax2.plot([dash_idx,dash_idx],  [-25e3, 25e3], color="black", linestyle=":")
	ax2.plot(x_arr, pll_f_arr1, label="f-1")
	#ax2.plot(x_arr, pll_f_arr2, label="f-2")
	ax2.plot(x_arr, f1_avg_series, color="blue", label="f-1-avg-tx")
	#ax2.plot(x_arr, f2_avg_series, color="red", label="f-2-avg-tx")
	ax2.grid()
	ax2.legend()


	ax3.set_title("extra")
	for dash_idx in dash_idxs:
		ax3.plot([dash_idx,dash_idx],  [-25e3, 25e3], color="black", linestyle=":")
	ax3.plot(x_arr, fdelta_1, label="f-1-delta")
	#ax3.plot(x_arr, fdelta_2, label="f-2-delta")
	ax3.plot(x_arr, fdelta_1_winstd_pseudo, color="blue", label="f-1-delta-std-window")
	#ax3.plot(x_arr, f2_delta_std_window, color="red", label="f-2-delta-std-window")
	ax3.grid()
	ax3.legend()

	fig.set_layout_engine("tight")


	plt.show()












def windowtst():
	from scipy.signal import windows
	win1 = windows.taylor(27, nbar=20, sll=100, norm=False)
	#win2 = windows.taylor(27, nbar=20, sll=100, norm=False)
	win1 = np.array(win1)
	#win2 = np.array(win2)

	#win1 = firwin(numtaps=71, cutoff=[0.45,0.55, 0.65,0.75], pass_zero=True)
	#win1 = firwin(numtaps=71, cutoff=[0.2, 0.6], pass_zero=True)

	#win_x1 = win1 * np.exp(1j*np.pi * np.arange(0, len(win1)) * 0.5)
	#win_x2 = win1 * np.exp(1j*np.pi * np.arange(0, len(win1)) * -0.33)
	#win2 = win_x1 + win_x2

	win2 = np.sinc(np.linspace(-1,1, len(win1))*6.0) * win1

	#win1 = np.roll(win1, np.random.randint(0, len(win1)))
	#win3 = np.roll(  np.concatenate( (np.zeros(1024), win2) ) , 0)

	A1 = np.fft.fft(win1, 2048) / (len(win1)/2.0)
	A2 = np.fft.fft(win2, 2048) / (len(win2)/2.0)
	#A3 = np.fft.fft(win3, 2048) / (len(win3)/2.0)
	freqs1 = np.linspace(-0.5, 0.5, len(A1))
	freqs2 = np.linspace(-0.5, 0.5, len(A2))
	#freqs3 = np.linspace(-0.5, 0.5, len(A3))
	response1 = 20 * np.abs(np.fft.fftshift(A1 / abs(A1).max()))
	response2 = 20 * np.abs(np.fft.fftshift(A2 / abs(A2).max()))
	#response3 = 20 * np.log(np.abs(np.fft.fftshift(A3 / abs(A3).max())))

	fig = plt.figure(figsize=(14,14))
	ax = fig.add_subplot(111)
	#ax.plot(freqs, win, label="window")
	ax.plot(freqs1,  response1, label="window 1")
	ax.plot(freqs2,  response2, label="window 2")
	#ax.plot(freqs3,  response3, label="window 3")
	ax.grid()
	ax.legend()
	fig.set_layout_engine("tight")
	plt.show()





if __name__ == '__main__':
	naive_pll_tst("B")
	#windowtst()













