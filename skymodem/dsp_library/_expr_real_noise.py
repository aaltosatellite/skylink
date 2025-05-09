import numpy as np
from matplotlib import pyplot as plt
from mtools.tools_dsp import waterfall_mx
#from mtools.tools_dsp import pll_cont_step, create_pll_statevector, winstd_init, pll_single_shot, pll_cont, pll_cont_strm
from numba import njit
import time
import pickle
from scipy.signal import firwin
import os

def gauss_noise(n, amp):
	return np.random.normal(0, amp, n) + 1j*np.random.normal(0, amp, n)


def histpoints(arr, nbins):
	bins = np.zeros(nbins)
	borders = np.linspace(np.min(arr), np.max(arr), nbins+1)
	#bins[0] = np.sum( arr < borders[1] ) * 1.0
	for i in range(0, nbins):
		bins[i] = np.sum( arr < borders[i+1] )* 1.0 - np.sum(bins[0:i])
	centers = (borders[1:] + borders[0:-1]) * 0.5
	assert len(centers) == nbins
	return centers, bins


def investigate_noise_shape():
	fpath = "/home/elmore/datasetit/radiotallenteet/uhf-nayte-{}.dat".format(76) #96, 76
	f = open(fpath, "rb")
	samples0 = pickle.loads(f.read())
	f.close()
	print("Loaded:")
	print("\t{} samples".format(len(samples0)))
	print(samples0.dtype)
	sr = 1e6
	symbolrate = 9600
	f_offset0 = 1.2445e5
	f_offset = 15.0e3
	nsamples = len(samples0)
	samples0 = samples0 / np.average(np.abs(samples0))

	#samples0 = samples0 * np.exp(2j*np.pi * np.arange(len(samples0)) * -f_offset0/sr )

	waterfall_mx(samples=samples0, fftlen=2048, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=False)
	clean_noise_gap_1 = [int(5.46e5), int(1.78e6)]
	clean_noise_gap_2 = [int(2.88e6), int(4.08e6)]
	noise1 = samples0[clean_noise_gap_1[0] : clean_noise_gap_1[1] ]
	noise2 = samples0[clean_noise_gap_2[0] : clean_noise_gap_2[1] ]
	len1 = len(noise1)
	len2 = len(noise2)

	lptaps = firwin(numtaps=201, cutoff=10e3/sr, pass_zero=True)
	lpassed1 = np.convolve(noise1, lptaps)[100:-100]
	lpassed2 = np.convolve(noise2, lptaps)[100:-100]
	cleaned1 = noise1 - lpassed1
	cleaned2 = noise2 - lpassed2
	avg_unclean1 = np.average(np.abs(noise1))
	avg_clean1 = np.average(np.abs(cleaned1))
	print("Avg abs of not-cleaned1: {}".format( round(avg_unclean1, 3) ))
	print("Avg abs of cleaned1:     {}".format( round(avg_clean1, 3) ))

	waterfall_mx(samples=noise1, fftlen=2048, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=False)
	#waterfall_mx(samples=noise2, fftlen=2048, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=False)
	waterfall_mx(samples=cleaned1, fftlen=2048, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=False)
	#waterfall_mx(samples=cleaned2, fftlen=2048, fft_jump=1024, srate=sr, plot_and_show=True, y_is_time=False)

	gaussnoise = gauss_noise(n=len(noise1)*10, amp=0.26)

	fig = plt.figure(figsize=(15,15))
	ax1 = fig.add_subplot(111)

	xbins1, bins1 = histpoints(np.abs(noise1), nbins=100)
	xbins2, bins2 = histpoints(np.abs(cleaned1), nbins=100)
	xbins_gauss, bins_gauss = histpoints(np.abs(gaussnoise), nbins=100)
	ax1.plot( xbins1, bins1 )
	ax1.plot( xbins2, bins2 )
	ax1.plot( xbins_gauss, bins_gauss*0.1 )
	ax1.grid()

	fig.set_layout_engine("tight")
	plt.show()






	sqrthalf = 0.5**0.5
	magic = 1 / (np.pi - 2)**2
	n_fft = 500
	fftlen = 2048
	fft_noise = np.zeros(fftlen)
	fft_cleaned = np.zeros(fftlen)
	fft_gauss1 = np.zeros(fftlen)
	fft_gauss2 = np.zeros(fftlen)
	for _ in range(n_fft):
		i0 = np.random.randint(0, len1 - fftlen)
		fft_noise += np.abs(np.fft.fftshift(np.fft.fft( noise1[i0:i0+fftlen] )))
		fft_cleaned += np.abs(np.fft.fftshift(np.fft.fft( cleaned1[i0:i0+fftlen] )))
		fft_gauss1 += np.abs(np.fft.fftshift(np.fft.fft( gauss_noise(fftlen, magic * avg_clean1) )))
		fft_gauss2 += np.abs(np.fft.fftshift(np.fft.fft( gauss_noise(fftlen, (sqrthalf/fftlen)**0.5 ) )))
	fft_noise = fft_noise / n_fft
	fft_cleaned = fft_cleaned / n_fft
	fft_gauss1 = fft_gauss1 / n_fft
	fft_gauss2 = fft_gauss2 / n_fft

	print("fftavg clean:   {}".format( np.average( fft_cleaned ) ))
	print("fftavg gauss1:  {}".format( np.average( fft_gauss1 ) ))
	print("ratio:          {}".format( np.average( fft_cleaned ) / np.average( fft_gauss1 ) ))

	amplitudes = np.abs(samples0)

	fig = plt.figure(figsize=(14,12))
	ax1 = fig.add_subplot(111)

	ax1.plot(np.arange(fftlen), fft_noise)
	ax1.plot(np.arange(fftlen), fft_cleaned)
	ax1.plot(np.arange(fftlen), fft_gauss1)
	ax1.plot(np.arange(fftlen), fft_gauss2)

	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()


	"""assert type(samples1) == np.ndarray, type(samples1)
	assert samples1.dtype in (np.complex128, np.complex64), samples1.dtype
	# CUT
	samples1 = samples1[4150000:4400000]
	samples1 = samples1 * np.exp(2j*np.pi * (1/sr) * (f_offset-f_offset0)*np.arange(len(samples1)))
	avgamp = np.average( np.abs(samples1) )
	print("\tavg amplitude: ",avgamp)
	samples1 = samples1 * (1/avgamp)
	avgamp = np.average( np.abs(samples1) )
	print("\tavg amplitude: ",avgamp)"""




def d_timing(n):
	dd = dict()
	for i in range(n):
		dd[os.urandom(200)] = time.time() - i*1.0

	t0 = time.perf_counter()
	for k in list(dd.keys()):
		if (time.time() - dd[k]) < n*0.5:
			del dd[k]
	dt = time.perf_counter() - t0
	print("{} entries in {} µs".format( n, round(1e6 * dt, 3) ))



investigate_noise_shape()

d_timing(1)
d_timing(2)
d_timing(4)
d_timing(10)
d_timing(20)
d_timing(30)
d_timing(60)
















