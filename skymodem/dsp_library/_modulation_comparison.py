import numpy as np
from kuokka.lib_tools import make_samples, DEFAULT_SYNCHWORD_BITS
from sdr_recorder import get_samples, fpaths
from mtools.tools_dsp import waterfall_mx
from scipy.signal import firwin
from matplotlib import pyplot as plt

def compare_generated_to_recording():
	fpath, fshift0, known_payloads = fpaths[7]
	rec_samples = get_samples(fpath)
	rec_samples = rec_samples * np.exp(2j*np.pi*np.arange(len(rec_samples)) * (fshift0+200)/1e6 )
	rec_samples = rec_samples[int(2.2e6):int(2.7e6)]
	waterfall_mx(samples=rec_samples, fftlen=2048, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=False)

	bitstring = np.random.randint(0,2, 256)*2 - 1
	bitstring[0:32] = [1,-1]*16
	bitstring[32:32+32] = DEFAULT_SYNCHWORD_BITS*2 -1
	sr0 = 1e6
	sps = sr0/9600
	gen_samples = make_samples(sps_f=sps, bitstring=bitstring, f_offset=0.0, power=1.0, mod_index=0.5, shaper_mode=1, shaper_BT_prod=0.425, shaper_n_taps=int(sps)*6+1)  #BT=0.425
	gen_samples = np.concatenate( (np.zeros(1000, dtype=np.complex128), gen_samples, np.zeros(1000, dtype=np.complex128) ) )

	lp_taps = firwin(numtaps=201, cutoff=0.9*9600/1e6, pass_zero=True)


	rec_lpd = np.convolve(rec_samples, lp_taps)
	rec_zz = rec_lpd * np.conj( np.roll(rec_lpd, 1) )
	rec_dmd = np.arctan2(rec_zz.imag, rec_zz.real)

	gen_lpd = np.convolve(gen_samples, lp_taps)
	gen_zz = gen_lpd * np.conj( np.roll(gen_lpd, 1) )
	gen_dmd = np.arctan2(gen_zz.imag, gen_zz.real)




	xx_rec = np.arange(len(rec_dmd))
	xx_gen = np.arange(len(gen_dmd)) + 47000 + 2795

	fig = plt.figure(figsize=(21,12))
	ax1 = fig.add_subplot(111)

	ax1.plot(xx_rec, rec_dmd)
	ax1.plot(xx_gen, gen_dmd)
	ax1.grid()

	fig.set_layout_engine("tight")
	plt.show()



compare_generated_to_recording()





