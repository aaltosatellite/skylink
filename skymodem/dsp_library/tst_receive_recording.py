import numpy as np
from kuokka.lib_receiver import ReceiverSettings, Receiver
from mtools.tools_dsp import create_resampler, resampler_execute
from mtools.tools_dsp import waterfall_mx
from matplotlib import pyplot as plt
import time, pickle
from scipy.signal import firwin

fpath0 = "/home/elmore/datasetit/radiotallenteet/uhf-nayte-76.dat"
fpath1 = "/home/elmore/datasetit/radiotallenteet/uhf-nayte-96.dat"

fpath2 = "/home/elmore/datasetit/radiotallenteet/uhf-nayte-472.dat"
fpath3 = "/home/elmore/datasetit/radiotallenteet/uhf-nayte-237.dat"

fpath4 = "/home/elmore/datasetit/radiotallenteet/uhf-817_437.0MHz-1000ksps.pickled"
fpath5 = "/home/elmore/datasetit/radiotallenteet/uhf-969_437.0MHz-1000ksps.pickled"
fpath6 = "/home/elmore/datasetit/radiotallenteet/uhf-965_437.0MHz-1000ksps.pickled"
fpath7 = "/home/elmore/datasetit/radiotallenteet/uhf-298_437.0MHz-1000ksps.pickled"

fpath8 = "/home/elmore/datasetit/radiotallenteet/uhf-447_437.0MHz-1000ksps.pickled"

fpath9 = "/home/elmore/datasetit/radiotallenteet/uhf-195_437.0MHz-1000ksps.pickled"


fpaths = [fpath0,fpath1, fpath2,fpath3,  fpath4,fpath5,fpath6]
def get_samples2(fpath):
	f = open(fpath, "rb")
	rd = f.read()
	f.close()
	samples = pickle.loads(rd)
	assert type(samples) == np.ndarray
	return samples





def draw_demod(samples):
	#samples = get_samples2(idx)
	#samples = samples[250000:-500000]
	sr0 = 1e6
	fshift = -0.12250e6 -500 #-86.5e3
	baudrate = 9600 * 1
	samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) * fshift/sr0)
	#samples = samples[100:len(samples)//3]
	waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, srate=sr0, plot_and_show=True, y_is_time=True)

	lp_cutoff = 0.630 * 2.5 * baudrate / sr0
	lpfilter = firwin(numtaps=201, cutoff=lp_cutoff, pass_zero=True)

	samples = np.convolve(samples, lpfilter)

	zz = samples * np.conj( np.roll(samples, 1) )
	dmd = np.arctan2(zz.imag, zz.real)

	xx0 = np.arange(len(dmd))
	xx_t = np.arange(len(dmd)) * (1/sr0)
	xx_sym = np.arange(len(dmd)) * (1/sr0) / (1/baudrate)

	fig = plt.figure(figsize=(13,13))
	ax1 = fig.add_subplot(111)

	ax1.plot(xx_sym, dmd )

	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()









def tst0():
	samples = get_samples2(fpath7)
	sr0 = 1e6
	nsamples = len(samples)
	samples = samples * np.exp(2j*np.pi * np.arange(nsamples) * (1/sr0) * -100e3)
	#samples = np.concatenate( (samples[0:300000], samples) )
	baudrate			= 9600			# tx param
	sps  				= 21			# todo measure final A against a spectrum of sps's....
	mod_index			= 0.5			# tx param
	BT 					= 0.5
	batch_maxlen 		= 6000
	f_tune				= 437.1e6
	f_signal			= 437.00e6 + 125e3

	settings = ReceiverSettings(sr0=sr0, baudrate=baudrate, bufferlen=3400000, batch_maxlen=batch_maxlen, f_tune=f_tune, f_expected=f_signal)
	settings.sps 					= sps
	settings.baudrate 				= baudrate
	settings.lp_cutoff_coeff 		= 0.625 #0.625
	settings.lp_ntaps				= 161
	settings.JPL_n_decay 			= 28.0
	settings.synch_delay_mpr 		= 22.0

	settings.rs_f_cutoff_coeff		= 0.499
	settings.m_halflen				= 25

	settings.fftlen					= 1024
	settings.jumplen				= 1024//2
	settings.mod_index				= mod_index
	settings.BT						= BT
	settings.mask_mode				= 1
	settings.c_stat_update			= 1 / 700
	settings.c_f_update_minimum 	= 0.02
	settings.T_f_upd_recovery 		= 4.0
	settings.fft_trigger_on_level 	= 6.5
	settings.fft_trigger_off_level 	= 2.0
	settings.start_margin_mpr		= 2.0
	settings.end_margin_mpr			= 1.4

	rx = Receiver(settings=settings)
	rx2 = Receiver(settings=settings)

	t0 = time.perf_counter()
	rx.switch_baudrate(baudrate=9600*2, sps=settings.sps)
	dt = (time.perf_counter() - t0)
	print("Baudrate switch in: {} s".format( round(dt, 3) ))

	rx.switch_baudrate(baudrate=9600, sps=settings.sps)


	if True:
		waterfall_mx(samples=samples, fftlen=1024, fft_jump=1024, srate=sr0, plot_and_show=True, y_is_time=True)

		fftstate = rx.FFTstatemx
		mask0 = np.zeros(1024)
		scan_idxs = np.int64(fftstate[7,:int(fftstate[0,10])])
		mask0[scan_idxs] = 1
		resampler = create_resampler(m_halflen=21, n_banks=64, r_rate=sps*baudrate/sr0, f_cutoff=0.499*sps*baudrate/sr0, allow_aliasing=False)
		samples_rs = resampler_execute(samples=samples, statemx=resampler)
		mx, extent, aspect = waterfall_mx(samples=samples_rs, fftlen=1024, fft_jump=1024, srate=sps*baudrate, plot_and_show=False, y_is_time=True)
		mx[10] = mask0
		mx[11] = mask0
		mx[12] = mask0
		fig = plt.figure(figsize=(14,14))
		ax = fig.add_subplot(111)
		ax.imshow(mx, origin="lower",  extent=extent, aspect=aspect)
		fig.set_layout_engine("tight")
		plt.show()

	batch0 = samples[0 : 400]
	rx.push_samples(batch=batch0, give_bits=True)
	rx.push_samples(batch=batch0, give_bits=True)

	bits = np.zeros(0, dtype=np.int64)
	pl_list = list()
	feed_head = 0
	dt_total = 0
	while feed_head < nsamples:
		batchlen = 8000
		batch = samples[feed_head : feed_head+batchlen]

		t0 = time.perf_counter()
		ret_pl = rx.push_samples(batch=batch, give_bits=False)
		dt_total += (time.perf_counter() - t0)

		ret_b = rx2.push_samples(batch=batch, give_bits=True)

		if ret_pl:
			pl_list.extend(ret_pl)
			t_abs = feed_head / sr0
			print("Extended with {} payloads at {} s".format( len(ret_pl),  round(t_abs, 2)) )

		bits = np.concatenate( (bits, ret_b) )
		feed_head += batchlen
	speed = nsamples / dt_total
	overmatch = speed / sr0
	budget_fraction	= (1/overmatch) / 0.5
	cpu_fraction	= (1/overmatch) / 1.0

	print("="*50)
	print("\tspeed:          {} Ms/s".format( round(1e-6 * speed, 2) ))
	print("\tovermatch:      {}".format( round(overmatch, 2) ))
	print("\tbudget use:     {} %".format( round( 100*budget_fraction , 2) ))
	print("\tcpu core use:   {} %".format( round( 100*cpu_fraction , 2) ))
	print("\t\tpart 1:            {} %".format( round( 100*rx.dt_array[0]/np.sum(rx.dt_array) , 2) ))
	print("\t\tpart 2:            {} %".format( round( 100*rx.dt_array[1]/np.sum(rx.dt_array) , 2) ))
	print("\t\tpart 3:            {} %".format( round( 100*rx.dt_array[2]/np.sum(rx.dt_array) , 2) ))
	print("\t\tpart 4:            {} %".format( round( 100*rx.dt_array[3]/np.sum(rx.dt_array) , 2) ))
	print("="*50)

	print("Got {} bits".format(len(bits)))
	print("Got {} payloads".format(len(pl_list)))

	for pl in pl_list:
		print(pl)
	xx = np.arange(len(rx.center_f_array))
	fig = plt.figure(figsize=(14,14))
	ax1 = fig.add_subplot(211)
	ax2 = fig.add_subplot(212)

	ax1.plot( xx, rx.center_f_array )
	ax1.grid()

	ax2.plot( xx, rx.fft_instr_array[:,0] )
	ax2.plot( xx, rx.fft_instr_array[:,1] )
	ax2.plot( xx, rx.fft_instr_array[:,2] )
	ax2.grid()

	fig.set_layout_engine("tight")
	plt.show()









tst0()
draw_demod(get_samples2(fpath8))























