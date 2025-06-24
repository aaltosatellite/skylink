from matplotlib import pyplot as plt
import numpy as np
import time, os
from sdr_recorder import get_samples, fpaths
from kuokka.lib_tools import radionoise, make_samples2
from mtools.tools_dsp import fft_usual, waterfall_mx
from mtools.tools_math import rollsmooth
from kuokka.lib_fft_finder import construct_fft_mask



def measure_ffts(samples, sr0, fftlen, n_stack, ifft_fist, ifft_last,   i_band_tup, i_noise_0):
	i_band_0,i_band_1 = i_band_tup
	i_noise_1 = i_noise_0 + (i_band_1-i_band_0)
	P_band = 0.0
	P_noise = 0.0
	fft = np.zeros(fftlen, dtype=np.float64)
	for _ in range(n_stack):
		#i = np.random.randint(fftlen, len(samples)-fftlen)
		i = ifft_fist + np.random.randint(0, (ifft_last-ifft_fist)-fftlen)
		fft_, freqs_ = fft_usual(iq_arr=samples[i:i+fftlen], srate=sr0, take_abs=True)
		P_band += np.sum(fft_[i_band_0:i_band_1]**2) / (fftlen**2)
		P_noise += np.sum(fft_[i_noise_0:i_noise_1]**2) / (fftlen**2)
		fft += fft_
		#freqs = freqs_
	fft = fft / n_stack
	P_band = P_band / n_stack
	P_noise = P_noise / n_stack
	return fft, P_band, P_noise






def main1(recording):
	if recording:
		fpath, fshift0, known_payloads = fpaths[7]
		samples = get_samples(fpath)
		sr0 = 1e6
		baudrate = 9600
		mod_index = 0.5
		samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) * (1/sr0) * (fshift0+350.0) )
		i_first1 = int(3.82e6)
		i_last1 = int(4.2e6)
		#i_first2 = int(6.94e6)
		#i_last2 = int(7.045e6)

	else:
		bitstring = np.random.randint(0,2, 1024*30)*2 - 1
		sr0 = 1e6
		baudrate = 9600*2
		mod_index = 0.5
		samples, _ = make_samples2(sps_f=sr0/baudrate, bitstring=bitstring, f_offset=0.0, power=1.0, mod_index=mod_index, shaper_BT_prod=0.5)
		i_first1 = 1
		i_last1 = len(samples) - int(sr0/baudrate)

	#samples = samples + radionoise(n=len(samples), sr=sr0, W_per_Hz=0.1/baudrate)
	#nsamples = len(samples)
	#waterfall_mx(samples=samples, fftlen=2048, fft_jump=2048, fft_stack=5, srate=sr0, plot_and_show=True, y_is_time=False)

	fftlen 		= 1024*2
	n_stack 	= 1024*2
	_, freqs 	= fft_usual(iq_arr=np.random.normal(0,1, fftlen)*1j +1, srate=sr0, take_abs=True)

	bw_multipliers = np.array([0.01, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.8], dtype=np.float64)
	P_band_bwm_arr = np.zeros(len(bw_multipliers))*0.0
	P_noise_bwm_arr = np.zeros(len(bw_multipliers))*0.0
	bw_arr = np.zeros(len(bw_multipliers))*0.0
	for impr in range(len(bw_multipliers)):
		bw 			= baudrate*bw_multipliers[impr]
		i_band_0 = [i for i in range(fftlen) if (freqs[i] > (-bw/2))][0]
		i_band_1 = [i for i in range(fftlen) if (freqs[i] >  (bw/2))][0]
		i_noise_0 = i_band_0 + int(fftlen*0.35) - int((i_band_1-i_band_0)*0.5)
		#i_noise_1 = i_noise_0 + int(i_band_1 - i_band_0)
		fft, P_band, P_noise = measure_ffts(samples=samples, sr0=sr0, fftlen=fftlen, n_stack=n_stack, ifft_fist=i_first1, ifft_last=i_last1, i_band_tup=(i_band_0, i_band_1), i_noise_0=i_noise_0)
		P_band_bwm_arr[impr]  = P_band
		P_noise_bwm_arr[impr] = P_noise
		bw_arr[impr] = i_band_1-i_band_0


	bw 			= baudrate * 0.75 * (mod_index/0.5)
	i_band_0 = [i for i in range(fftlen) if freqs[i] > -bw/2][0]
	i_band_1 = [i for i in range(fftlen) if freqs[i] >  bw/2][0]
	#int(0.5 * 1.5 * fftlen / sps)*2 + 1
	bw_mask = 1.5 * baudrate
	i_mask_0 = [i for i in range(fftlen) if freqs[i] > -bw_mask/2][0]
	i_mask_1 = [i for i in range(fftlen) if freqs[i] >  bw_mask/2][0]
	masklen = i_mask_1 - i_mask_0

	i_noise_0 = i_band_0 + int(fftlen*0.35) - int((i_band_1-i_band_0)*0.5)
	i_noise_1 = i_noise_0 + int(i_band_1 - i_band_0)
	fft, P_band, P_noise = measure_ffts(samples=samples, sr0=sr0, fftlen=fftlen, n_stack=n_stack*20, ifft_fist=i_first1, ifft_last=i_last1, i_band_tup=(i_band_0, i_band_1), i_noise_0=i_noise_0)

	#fft = rollsmooth(fft, n=3, stride=1)
	empiric_mask = construct_fft_mask(sps=sr0/baudrate, mod_index=mod_index, BT_rx_match=0.5, fftlen=fftlen, masklen=masklen, nn=2000)

	fig = plt.figure(figsize=(17,14))
	ax1 = fig.add_subplot(211)
	ax2 = fig.add_subplot(212)
	#ax1.plot(np.linspace(0, len(fft)-1, len(fft)), fft)
	ax1.plot(freqs, fft)
	ax1.plot(freqs[i_mask_0:i_mask_1], empiric_mask * np.max(fft))
	ax1.plot([freqs[i_band_0], freqs[i_band_0]], [0, np.max(fft)], color="black")
	ax1.plot([freqs[i_band_1], freqs[i_band_1]], [0, np.max(fft)], color="black")

	ax1.plot([freqs[i_mask_0], freqs[i_mask_0]], [0, np.max(fft)], color="red")
	ax1.plot([freqs[i_mask_1], freqs[i_mask_1]], [0, np.max(fft)], color="red")

	ax1.plot([freqs[i_noise_0], freqs[i_noise_0]], [0, np.max(fft)], color="black")
	ax1.plot([freqs[i_noise_1], freqs[i_noise_1]], [0, np.max(fft)], color="black")
	ax1.grid()

	ax2.plot(bw_multipliers, P_band_bwm_arr)
	ax2.plot(bw_multipliers, P_noise_bwm_arr)
	ax2.plot(bw_multipliers, np.log10(P_band_bwm_arr/P_noise_bwm_arr)*10)
	ax2.plot(bw_multipliers, bw_arr)
	ax2.grid()






	fig.set_layout_engine("tight")
	plt.show()




main1(recording=False)























