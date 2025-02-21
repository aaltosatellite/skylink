import numpy as np
from kuokka.lib_receiver import ReceiverSettings, Receiver
from mtools.tools_dsp import create_resampler, resampler_execute
from mtools.tools_dsp import waterfall_mx
from matplotlib import pyplot as plt
import time
from _draw_lab_spams import get_samples2


def tst0():

	samples = get_samples2(0)
	sr0 = 1e6
	nsamples = len(samples)
	samples = samples * np.exp(2j*np.pi * np.arange(nsamples) * (1/sr0) * -100e3)

	baudrate			= 9600			# tx param
	sps  				= 21			# todo measure final A against a spectrum of sps's....
	mod_index			= 0.5			# tx param
	batch_maxlen 		= 6000
	f_tune				= 437.1e6
	f_signal			= 437.00e6 + 125e3

	settings = ReceiverSettings(sr0=sr0, baudrate=baudrate, bufferlen=600000, batch_maxlen=batch_maxlen, f_tune=f_tune, f_expected=f_signal)
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
	settings.mask_mode				= 1
	settings.c_stat_update			= 1 / 700
	settings.c_f_update_minimum 	= 0.02
	settings.T_f_upd_recovery 		= 2.5
	settings.fft_trigger_on_level 	= 4.9
	settings.fft_trigger_off_level 	= 3.0
	settings.start_margin_mpr		= 1.0
	settings.end_margin_mpr			= 0.4

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
		batchlen = np.random.randint(0, batch_maxlen)
		batch = samples[feed_head : feed_head+batchlen]

		t0 = time.perf_counter()
		ret_pl = rx.push_samples(batch=batch, give_bits=False)
		dt_total += (time.perf_counter() - t0)

		ret_b = rx2.push_samples(batch=batch, give_bits=True)

		if ret_pl:
			pl_list.extend(ret_pl)

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



tst0()