import numpy as np
from kuokka.lib_framing import frame_packet
from kuokka.lib_reedsolomon import get_default_rs
from mtools.tools_dsp import resampler_execute, create_resampler
from kuokka.lib_tools import DEFAULT_SYNCHWORD
from kuokka.lib_receiver import Receiver, ReceiverSettings
from mtools.tools_dsp import waterfall_mx
from kuokka.lib_tools import make_samples, radionoise
import time
from matplotlib import pyplot as plt

"""
savior_params["sps"] 				= 21		# param ~
savior_params["JPLdecay"] 			= 32.0		# param ~
savior_params["synch_delay_mpr"] 	= 22.0		# param ~
savior_params["lp_cutoff_coeff"] 	= 0.625		# param !
savior_params["lp_ntaps"] 			= 121		# param ~
savior_params["BT"] 				= -1.0		# param !
savior_params["mod_index"] 			= 0.7		# param !
"""

"""
	#============================================
	fftlen			= 1024			# !		(1024,  2048, 512)
	jumplen			= 1024//2		# !		(fftlen / [2,3,4])
	c_stat_update	= 1 / 700		# -		([400:2000])
	c_f_update		= 1.0			# !
	tx_trigger_lvl	= 4.5			# !!!	(4.5 < _ < 9)
	masklen			= int(fftlen/sps)			# !		( int( [0.8:1.2] * fftlen/sps)  )
	mask_c_array	= np.array((1.0, 0.0, 0.0))		# !!	( -1 <= _ <= 1 )
	start_margin_mpr= 1.0			# !!!
	end_margin_mpr  = 0.0			# !
	#============================================
"""



def tst0():
	from _draw_lab_spams import get_samples, get_samples2
	samples = get_samples2(0)
	sr0 = 1e6
	nsamples = len(samples)
	samples = samples * np.exp(2j*np.pi * np.arange(nsamples) * (1/sr0) * -100e3)

	baudrate			= 9600			# tx param
	sps  				= 27			# todo measure final A against a spectrum of sps's....
	mod_index			= 0.7			# tx param
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



	bits = np.zeros(0, dtype=np.int64)
	pl_list = list()
	feed_head = 0
	dt_total = 0
	while feed_head < nsamples:
		batchlen = np.random.randint(0, batch_maxlen)
		batch = samples[feed_head : feed_head+batchlen]

		t0 = time.perf_counter()
		ret = rx.push_samples(batch=batch, give_bits=True)
		dt_total += (time.perf_counter() - t0)

		ret2 = rx2.push_samples(batch=batch, give_bits=False)
		if ret2:
			pl_list.extend(ret2)

		bits = np.concatenate( (bits, ret) )
		feed_head += batchlen
	speed = nsamples / dt_total
	overmatch = speed / sr0
	budget_fraction	= (1/overmatch) / 0.5

	print("Got {} bits".format(len(bits)))
	print("Got {} payloads".format(len(pl_list)))





def tst1(n_packets, do_waterfall=False, do_print=False, do_plots=False):
	sr0					= 1.0e6
	baudrate			= 9600		# tx param
	sps  				= 17		# todo measure final A against a spectrum of sps's....
	mod_index			= 0.5		# tx param
	BT_prod				= -1.0		# tx param
	nbits 				= 32 + 24 + 8*(32 + 120)  # = 1272
	noiseamp 			= 0.020 / 9600
	batch_maxlen 		= 6000
	f_tune				= 437.0e6
	f_signal			= 437.00e6 - 25e3
	f_doppler			= f_signal * ((3e8 + 7500) / 3e8) - f_signal
	f_offset0_rel		= (f_signal - f_tune) / sr0


	settings = ReceiverSettings(sr0=sr0, baudrate=baudrate, bufferlen=600000, batch_maxlen=batch_maxlen, f_tune=f_tune, f_expected=f_signal-f_doppler*0.5)
	settings.sps 					= sps
	settings.baudrate 				= baudrate
	settings.lp_cutoff_coeff 		= 0.625
	settings.lp_ntaps				= 121
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



	tgen0 = time.perf_counter()
	r_ratio				= sps * baudrate / sr0
	sps0 				= sr0 / baudrate
	nnoise1 = int(1.2 * (1/10) * (1/settings.c_stat_update) * settings.jumplen * sr0 / (settings.sps * settings.baudrate))
	samples = np.zeros(nnoise1, dtype=np.complex128)
	bitstrings = list()
	for i_packet in range(n_packets):
		bitstring 			= np.random.randint(0, 2, nbits)*2 - 1
		signal = make_samples(sps_f=sps0, bitstring=bitstring, f_offset=f_offset0_rel, power=1, mod_index=mod_index, shaper_mode=0, shaper_BT_prod=BT_prod, shaper_n_taps=int(10*sps0)+1)
		npad = int(len(signal) * 0.05)
		pad = np.zeros(npad, dtype=np.complex128)
		samples = np.concatenate( (samples, signal, pad) )
		bitstrings.append(bitstring)
	endpad = np.zeros(int(nnoise1/3), dtype=np.complex128)
	samples = np.concatenate( (samples, endpad) )
	nsamples = len(samples)
	samples = samples + radionoise(n=nsamples, sr=sr0, W_per_Hz=noiseamp)
	if do_print:
		print("\t{} samples.  {} buffers".format(nsamples , round(r_ratio * nsamples / settings.bufferlen, 1) ))
		print("\tCorresponding to {} s".format( round(nsamples/sr0, 2) ))
		print("\tgenerated in {} ms".format( round( 1e3*(time.perf_counter()-tgen0), 0 ) ))
		print("")

	rx = Receiver(settings=settings)


	if do_waterfall:
		waterfall_mx(samples=samples, fftlen=1024, fft_jump=1024, srate=sr0, plot_and_show=True, y_is_time=True)

		fftstate = rx.FFTstatemx
		mask0 = np.zeros(1024)
		scan_idxs = np.int64(fftstate[7,:int(fftstate[0,10])])
		print("scan indexes:",scan_idxs)
		mask0[scan_idxs] = 1
		resampler = create_resampler(m_halflen=21, n_banks=64, r_rate=sps*baudrate/sr0, f_cutoff=0.499*sps*baudrate/sr0, allow_aliasing=False)
		samples_rs = resampler_execute(samples=samples, statemx=resampler)
		mx, extent, aspect = waterfall_mx(samples=samples_rs, fftlen=1024, fft_jump=1024, srate=sps*baudrate, plot_and_show=False, y_is_time=True)
		mx[100] = mask0
		mx[101] = mask0
		mx[102] = mask0
		fig = plt.figure(figsize=(14,14))
		ax = fig.add_subplot(111)
		ax.imshow(mx, origin="lower",  extent=extent, aspect=aspect)
		fig.set_layout_engine("tight")
		plt.show()



	bits = np.zeros(0, dtype=np.int64)
	feed_head = 0
	dt_total = 0
	while feed_head < nsamples:
		batchlen = np.random.randint(0, batch_maxlen)
		batch = samples[feed_head : feed_head+batchlen]
		t0 = time.perf_counter()
		ret = rx.push_samples(batch=batch, give_bits=True)
		dt_total += (time.perf_counter() - t0)
		bits = np.concatenate( (bits, ret) )
		feed_head += batchlen
	speed = nsamples / dt_total
	overmatch = speed / sr0
	budget_fraction	= (1/overmatch) / 0.5

	match_sums = list()
	errorcounts = list()
	for i_packet in range(n_packets):
		corr = np.correlate(bits, bitstrings[i_packet])
		maxcorr = np.max(corr)
		match_sum = nbits - (nbits-maxcorr)//2
		match_sums.append(match_sum)
		errorcounts.append( nbits - match_sum )

	if do_print:
		print("\t{} bits output".format( len(bits) ))
		print("="*50)
		print("\tspeed:          {} Ms/s".format( round(1e-6 * speed, 2) ))
		print("\tovermatch:      {}".format( round(overmatch, 2) ))
		print("\tbudget use:     {} %".format( round( 100*budget_fraction , 2) ))
		print("="*50)
		for i_packet in range(n_packets):
			print("\t#{} match sum:    {} ({} %) ({} errors)".format(i_packet, match_sums[i_packet],  round(100* match_sums[i_packet] / nbits,1), errorcounts[i_packet] ))
		print("")


	avg_arr = rx.fft_instr_array[:,0]
	var_arr = rx.fft_instr_array[:,1]
	fftmax_arr = rx.fft_instr_array[:,2]
	if do_plots:
		fig = plt.figure(figsize=(14,11))
		ax1 = fig.add_subplot(311)
		ax2 = fig.add_subplot(312)
		ax3 = fig.add_subplot(313)

		ax1.plot(rx.center_f_array * sps * baudrate)
		ax1.grid()

		ax2.plot(avg_arr)
		ax2.plot(var_arr)
		ax2.grid()

		ax3.plot(fftmax_arr)
		ax3.grid()

		fig.set_layout_engine("tight")
		plt.show()

	return np.array(match_sums), np.array(errorcounts)



def round_1():
	n_packets = 16
	n_rep = 20
	errorcounts = np.zeros(n_packets) * 0.0
	for _ in range(n_rep):
		ms_, ec_ = tst1(n_packets=n_packets, do_waterfall=False, do_print=True, do_plots=False)
		errorcounts += ec_
	errorcounts = errorcounts / n_rep

	fig = plt.figure(figsize=(14,11))
	ax1 = fig.add_subplot(211)
	ax2 = fig.add_subplot(212)

	ax1.plot(errorcounts)
	ax1.grid()

	ax2.grid()


	fig.set_layout_engine("tight")
	plt.show()













def tst2_pl_mode(n_packets, do_waterfall=False, do_print=False, do_plots=False):
	sr0					= 1.0e6
	baudrate			= 9600		# tx param
	sps  				= 12		# todo measure final A against a spectrum of sps's....
	mod_index			= 0.7		# tx param
	BT_prod				= -1.0		# tx param
	noiseamp 			= 0.20 / 9600
	batch_maxlen 		= 6000
	f_tune				= 437.1e6
	f_signal			= f_tune + 21e3
	f_offset0_rel		= (f_signal - f_tune) / sr0

	settings = ReceiverSettings(sr0=sr0, baudrate=baudrate, bufferlen=600000, batch_maxlen=batch_maxlen, f_tune=f_tune, f_expected=f_signal)
	settings.m_halflen				= 15

	settings.sps 					= sps
	settings.baudrate 				= baudrate
	settings.lp_cutoff_coeff 		= 0.625
	settings.lp_ntaps				= 121
	settings.JPL_n_decay 			= 28.0
	settings.synch_delay_mpr 		= 22.0

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


	rs_mx, rs_cfg = get_default_rs()

	tgen0 = time.perf_counter()
	r_ratio				= sps * baudrate / sr0
	sps0 				= sr0 / baudrate
	nnoise1 = int(1.2 * (1/10) * (1/settings.c_stat_update) * settings.jumplen * sr0 / (settings.sps * settings.baudrate))
	samples = np.zeros(nnoise1, dtype=np.complex128)
	payloads = list()
	for i_packet in range(n_packets):
		pl_ints = np.random.randint(0, 256, 100)
		bitstring = frame_packet(pl=pl_ints, synchword_int=DEFAULT_SYNCHWORD, synchword_len=32, use_scrambler=True, use_rs=True, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=False)
		bitstring = np.concatenate( (np.array((1,0,1,0,1,0,1,0,1,0)), bitstring) )
		bitstring = bitstring*2 -1
		signal = make_samples(sps_f=sps0, bitstring=bitstring, f_offset=f_offset0_rel, power=1, mod_index=mod_index, shaper_mode=0, shaper_BT_prod=BT_prod, shaper_n_taps=int(10*sps0)+1)
		npad = int(len(signal) * 0.05)
		pad = np.zeros(npad, dtype=np.complex128)
		samples = np.concatenate( (samples, signal, pad) )
		payloads.append( bytes(list(pl_ints)) )
	endpad = np.zeros(int(nnoise1/3), dtype=np.complex128)
	samples = np.concatenate( (samples, endpad) )
	nsamples = len(samples)
	samples = samples + radionoise(n=nsamples, sr=sr0, W_per_Hz=noiseamp)
	if do_print:
		print("\t{} samples.  {} buffers".format(nsamples , round(r_ratio * nsamples / settings.bufferlen, 1) ))
		print("\tCorresponding to {} s".format( round(nsamples/sr0, 2) ))
		print("\tgenerated in {} ms".format( round( 1e3*(time.perf_counter()-tgen0), 0 ) ))
		print("")

	if do_waterfall:
		waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, srate=sr0, plot_and_show=True, y_is_time=True)

	rx = Receiver(settings=settings)

	recvd_payloads = list()
	feed_head = 0
	dt_total = 0
	while feed_head < nsamples:
		batchlen = np.random.randint(1000, batch_maxlen)
		batch = samples[feed_head : feed_head+batchlen]
		feed_head += batchlen
		t0 = time.perf_counter()
		ret = rx.push_samples(batch=batch, give_bits=False)
		dt_total += (time.perf_counter() - t0)
		if not (ret is None):
			recvd_payloads.extend(ret)
			assert type(ret[-1]) == bytes
	speed = nsamples / dt_total
	overmatch = speed / sr0
	core_fraction	= (1/overmatch) / 1.00
	budget_fraction	= (1/overmatch) / 0.5

	ratio = len(recvd_payloads) / n_packets

	if do_print:
		print("="*50)
		print("\tspeed:          {} Ms/s".format( round(1e-6 * speed, 2) ))
		print("\tovermatch:      {}".format( round(overmatch, 2) ))
		print("\tcore use:       {} %".format( round( 100*core_fraction , 2) ))
		print("\tbudget use:     {} %".format( round( 100*budget_fraction , 2) ))
		print("="*50)
		print("{} / {} payloads received. ({} %)".format(len(recvd_payloads), n_packets, round(100*ratio,1) ))
		for i_pl in range(n_packets):
			print("\t#{}:  {}".format(i_pl, str( payloads[i_pl] in recvd_payloads ) ))
		print("")


	avg_arr = rx.fft_instr_array[:,0]
	var_arr = rx.fft_instr_array[:,1]
	fftmax_arr = rx.fft_instr_array[:,2]
	if do_plots:
		fig = plt.figure(figsize=(14,11))
		ax1 = fig.add_subplot(311)
		ax2 = fig.add_subplot(312)
		ax3 = fig.add_subplot(313)

		ax1.plot(rx.center_f_array * sps * baudrate)
		ax1.grid()

		ax2.plot(avg_arr)
		ax2.plot(var_arr)
		ax2.grid()

		ax3.plot(fftmax_arr)
		ax3.grid()

		fig.set_layout_engine("tight")
		plt.show()

	return len(recvd_payloads), ratio


















tst0()

#tst1(n_packets=16, do_waterfall=True, do_print=True, do_plots=True)
#print("----------------------")
#tst2_pl_mode(n_packets=16, do_waterfall=True, do_print=True, do_plots=True)















