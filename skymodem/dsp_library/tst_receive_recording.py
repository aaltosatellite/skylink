import numpy as np
from kuokka.lib_receiver import ReceiverConfig, Receiver, precompile_receiver
from mtools.tools_dsp import create_resampler, resampler_execute
from mtools.tools_dsp import waterfall_mx
from matplotlib import pyplot as plt
import time
from sdr_recorder import get_samples, fpaths







def receive_a_recording():
	fpath, fshift0, known_payloads = fpaths[7]
	samples = get_samples(fpath)
	prepend = np.concatenate( (samples[0:8000],)*int(1.8*1e6/8000.0) )
	samples = np.concatenate( (prepend, samples) )
	sr0 = 1e6
	nsamples = len(samples)

	#samples = np.concatenate( (samples[0:300000], samples) )
	baudrate			= 9600			# tx param
	#sps  				= 10			# todo measure final A against a spectrum of sps's....
	mod_index			= 0.5			# tx param
	BT_rx_match 		= 0.425
	batch_maxlen 		= 1024*8
	f_tune				= 437.10e6
	f_center			= 437.00e6 + 125e3

	samples = samples * np.exp(2j*np.pi * np.arange(nsamples) * (1/sr0) * (fshift0+(f_center-f_tune)))


	rx_config = ReceiverConfig(sr0=sr0, baudrate=baudrate, bufferlen=3400000, batch_maxlen=batch_maxlen, f_tune=f_tune, f_center=f_center)
	#rx_config.sps 					= sps
	rx_config.mod_index				= mod_index
	rx_config.BT_rx_match			= BT_rx_match
	sps = rx_config.sps

	expected_relative_f = (f_center-f_tune) / (baudrate*sps)

	precompile_receiver(rx_config=rx_config, do_print=True)
	rx = Receiver(config=rx_config)
	rx2 = Receiver(config=rx_config)
	print("RX picked fftlen of {}".format(rx.get_fftlen()))

	t0 = time.perf_counter()
	rx.switch_baudrate(baudrate=9600*2, sps=rx_config.sps)
	dt = (time.perf_counter() - t0)
	rx.switch_baudrate(baudrate=9600, sps=rx_config.sps)
	print("Baudrate switch in: {} ms".format( round(dt*1e3, 1) ))

	if True:
		fftlen = rx.get_fftlen()
		waterfall_mx(samples=samples, fftlen=2048, fft_jump=2048//2, srate=sr0, plot_and_show=True, y_is_time=True)
		fftstate = rx.FFTstatemx
		mask0 = fftstate[6,:]
		resampler = create_resampler(m_halflen=21, n_banks=64, r_rate=sps*baudrate/sr0, f_cutoff=0.499*sps*baudrate/sr0, allow_aliasing=False)
		samples_rs = resampler_execute(samples=samples, statemx=resampler)
		mx, extent, aspect = waterfall_mx(samples=samples_rs, fftlen=fftlen, fft_jump=fftlen//2, srate=sps*baudrate, plot_and_show=False, y_is_time=True)
		mx[10] = mask0
		mx[11] = mask0
		mx[12] = mask0
		mx[13] = mask0
		fig = plt.figure(figsize=(14,14))
		ax = fig.add_subplot(111)
		ax.imshow(mx, origin="lower",  extent=extent, aspect=aspect)
		fig.set_layout_engine("tight")
		plt.show()

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
	print("speed:          {} Ms/s".format( round(1e-6 * speed, 2) ))
	print("overmatch:      {}".format( round(overmatch, 2) ))
	print("budget use:     {} %".format( round( 100*budget_fraction , 2) ))
	print("cpu core use:   {} %".format( round( 100*cpu_fraction , 2) ))
	for i_dt in range(len(rx.dt_array)):
		print("\tpart {}:            {} %".format(i_dt+1, round( 100*rx.dt_array[i_dt]/np.sum(rx.dt_array) , 2) ))
	print("parts of total:    {} %".format( round( 100*np.sum(rx.dt_array)/dt_total , 2) ))
	print("="*50)

	print("Got {} bits".format(len(bits)))
	print("Got {} payloads".format(len(pl_list)))
	print("with avg length of {}".format( np.average([len(x[0]) for x in pl_list]) ))
	print("(from {} to {})".format( np.min([len(x[0]) for x in pl_list]), np.max([len(x[0]) for x in pl_list]) ))
	print("Relative freq should be ~{}".format( round(expected_relative_f, 4) ))

	for pl_bytes, pl_f in pl_list:
		print(round(1e-6*pl_f, 4), ":", len(pl_bytes), pl_bytes)
	xx = np.arange(len(rx.center_f_array)) * 1000.0/(rx.config.sps*rx.config.baudrate)
	x_t_s0_r10 = np.arange(len(samples[::20])) * 20 * 1000.0/sr0
	fig = plt.figure(figsize=(14,14))
	ax1 = fig.add_subplot(211)
	ax2 = fig.add_subplot(212)

	ax1.plot(xx, rx.center_f_array)
	ax1.plot(xx, np.abs(rx.rs_array) / np.max(np.abs(rx.rs_array)))
	#ax1.plot( x_t_s0_r10, np.abs(samples[::20]) / np.max(np.abs(samples[::20])) )
	ax1.grid()

	ax2.plot(xx, rx.fft_instr_array[:,0] )
	ax2.plot(xx, rx.fft_instr_array[:,1] )
	ax2.plot(xx, rx.fft_instr_array[:,2] )
	ax2.grid()

	fig.set_layout_engine("tight")
	plt.show()








if __name__ == '__main__':
	receive_a_recording()























