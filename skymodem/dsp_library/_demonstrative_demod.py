import numpy as np
from matplotlib import pyplot as plt
from scipy.signal import firwin
from mtools.tools_dsp import waterfall_mx
import os
from numba import njit

Dataset_filepaths = ["/home/elmore/datasetit/AIS/recording_02-06-2022-18-40-162M-288k",
					 "/home/elmore/datasetit/AIS/recording_03-06-2022-10-40-162M-288k",
					 "/home/elmore/datasetit/AIS/recording_03-06-2022-10-45-162M-288k",
					 "/home/elmore/datasetit/AIS/recording_03-06-2022-10-46-162M-288k",
					 "/home/elmore/datasetit/AIS/recording_03-06-2022-10-47-162M-288k",
					 ]

def read_file(fpath):
	assert os.path.isfile(fpath)
	f = open(fpath, "r")
	rd = f.read()
	f.close()
	lines = rd.split("\n")
	samples = list()
	for line in lines:
		if not line.strip():
			continue
		samples.append( complex(line) )
	samples = np.array(samples, dtype=np.complex128)
	return samples



def demodulate(samples, sr, f_shift, symbolrate):
	samples1 = samples[int(2.4e6):int(2.6e6)]
	samples2 = samples[int(8.3e6):int(8.5e6)]
	samples = np.concatenate( (samples1,samples2) )

	lowpass_cutoff_coeff = 0.62 * 1.1
	lp_filter = firwin(numtaps=201, cutoff=symbolrate*lowpass_cutoff_coeff, fs=sr)
	waterfall_mx(samples=samples, fftlen=2048, fft_jump=1000, srate=sr, plot_and_show=True, y_is_time=False)
	nsamples = len(samples)
	shifted = samples * np.exp(2j*np.pi * np.arange(nsamples) * (1/sr) * f_shift)

	waterfall_mx(samples=shifted, fftlen=2048, fft_jump=1000, srate=sr, plot_and_show=True, y_is_time=False)

	lpd = np.convolve(shifted, lp_filter)

	dm_zz = lpd * np.conj(np.roll(lpd, 1))
	dmd = np.arctan2(dm_zz.imag, dm_zz.real)

	fig = plt.figure(figsize=(14,14))
	ax = fig.add_subplot(111)
	ax.plot(dmd)
	fig.set_layout_engine("tight")
	plt.show()



#samples_ = np.fromfile(Dataset_filepaths[1], dtype=np.complex64)
#demodulate(samples=samples_, sr=288e3, f_shift=2.501e4, symbolrate=9600)


@njit(cache=True)
def foobar(arr_in, ii0, nsamp, fillstart):
	arr_in[ii0:ii0+nsamp] = np.arange(fillstart,fillstart+nsamp)

a = np.zeros( (9,9) )

foobar(arr_in=a[0,:], ii0=2, nsamp=5, fillstart=100)
foobar(arr_in=a[4,:], ii0=2, nsamp=5, fillstart=500)
foobar(arr_in=a[:,4], ii0=2, nsamp=5, fillstart=100)

print(a)













