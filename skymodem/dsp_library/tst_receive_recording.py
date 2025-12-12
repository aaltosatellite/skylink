import numpy as np
from kuokka.lib_receiver import RXDSPConfig, Receiver, precompile_receiver
from mtools.tools_dsp import create_resampler, resampler_execute
from mtools.tools_dsp import waterfall_mx
from kuokka.lib_tools import radionoise
from matplotlib import pyplot as plt
import time
from sdr_recorder import get_samples, fpaths
from mtools.tools_math import rollsmooth






def receive_a_recording():
    fpath, fshift0, known_payloads = fpaths[7]
    samples = get_samples(fpath)
    #prepend = np.concatenate( (samples[0:8000],)*int(0.5*1e6/8000.0) )
    #samples = np.concatenate( (prepend, samples) )
    sr0 = 1e6
    samples = samples + radionoise(n=len(samples), sr=sr0, W_per_Hz=0.13/9600)
    nsamples = len(samples)

    #samples = np.concatenate( (samples[0:300000], samples) )
    baudrate			= 9600			# tx param
    #sps  				= 10			# todo measure final A against a spectrum of sps's....
    mod_index			= 0.5			# tx param
    BT_rx_match 		= 0.425
    batch_maxlen 		= 1024*8
    f_tune				= 437.060e6
    f_center			= 437.00e6 + 125e3

    samples = samples * np.exp(2j*np.pi * np.arange(nsamples) * (1/sr0) * (fshift0+(f_center-f_tune)))


    rx_config = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_center_frequency=f_center, baudrate=baudrate, bufferlen=2000000, batch_maxlen=batch_maxlen)
    #rx_config.sps 					= sps
    rx_config.mod_index				= mod_index
    rx_config.BT_rx_match			= BT_rx_match
    sps = rx_config.sps

    expected_relative_f = (f_center-f_tune) / (baudrate*sps)

    precompile_receiver(dsp_config=rx_config, do_print=True)
    rx = Receiver(config=rx_config)
    #rx2 = Receiver(config=rx_config)
    print("RX picked fftlen of {}".format(rx.get_fftlen()))

    t0 = time.perf_counter()
    rx.switch_baudrate(baudrate=9600*1, sps=rx_config.sps)
    dt = (time.perf_counter() - t0)
    #rx.switch_baudrate(baudrate=9600, sps=rx_config.sps)
    print("Baudrate switch in: {} ms".format( round(dt*1e3, 1) ))

    if np.sqrt(2) > 100:
        fftlen = rx.get_fftlen()
        waterfall_mx(samples=samples, fftlen=2048, fft_jump=2048//2, fft_stack=1, srate=sr0, plot_and_show=True, y_is_time=True)
        fftstate = rx.FFTstatemx
        mask0 = fftstate[6,:]
        resampler = create_resampler(m_halflen=21, n_banks=64, r_rate=sps*baudrate/sr0, f_cutoff=0.499*sps*baudrate/sr0, allow_aliasing=False)
        samples_rs = resampler_execute(samples=samples, statemx=resampler)
        mx, extent, aspect = waterfall_mx(samples=samples_rs, fftlen=fftlen, fft_jump=fftlen//2, fft_stack=1, srate=sps*baudrate, plot_and_show=False, y_is_time=True)
        mx[10] = mask0
        mx[11] = mask0
        mx[12] = mask0
        mx[13] = mask0
        fig = plt.figure(figsize=(14,14))
        ax = fig.add_subplot(111)
        ax.imshow(mx, origin="lower",  extent=extent, aspect=aspect)
        fig.set_layout_engine("tight")
        plt.show()

    pl_list = list()
    feed_head = 0
    dt_total = 0
    batchlen = 1024 * 4
    carrier_sense_array = list()
    carrier_sensed = 0
    while feed_head < nsamples:
        batch = samples[feed_head : feed_head+batchlen]
        carrier_sensed_old = carrier_sensed
        t0 = time.perf_counter()
        ret_pl, carrier_sensed = rx.process_batch(batch=batch, give_bits=False)
        dt_total += (time.perf_counter() - t0)
        carrier_sense_array.append( (feed_head+batchlen, carrier_sensed) )
        if carrier_sensed > carrier_sensed_old:
            print("(carrier up)")
        #ret_b, _ = rx2.process_batch(batch=batch, give_bits=True)

        if ret_pl:
            pl_list.extend(ret_pl)
            t_abs = feed_head / sr0
            print("Extended with {} payloads at {} s".format( len(ret_pl),  round(t_abs, 2)) )

        #bits = np.concatenate( (bits, ret_b) )
        feed_head += batchlen
    carrier_sense_array = np.array(carrier_sense_array)
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
        txt1 = "  part {: ^{width1}} ({}):".format(i_dt, rx.dt_array_names[i_dt], width1=2)
        txt2 = "{}{}".format(" "*(max(0, 27-len(txt1))), round( 100*rx.dt_array[i_dt]/np.sum(rx.dt_array) , 2))
        print(txt1 + txt2)
    print("parts of total:    {} %".format( round( 100*np.sum(rx.dt_array)/dt_total , 2) ))
    print("="*50)

    #print("Got {} bits".format(len(bits)))
    print("Got {} payloads".format(len(pl_list)))
    print("with avg length of {}".format( np.average([len(x[0]) for x in pl_list]) ))
    #print("(from {} to {})".format( np.min([len(x[0]) for x in pl_list]), np.max([len(x[0]) for x in pl_list]) ))
    print("Relative freq should be ~{}".format( round(expected_relative_f, 4) ))

    for pl_bytes, pl_f, pl_pt in pl_list:
        p_pl, p_noise, bw_p = pl_pt
        print(round(1e-6*pl_f, 4), ":", round(np.log10(p_pl/p_noise)*10.0,1), ":", bw_p, ":", len(pl_bytes), pl_bytes)
    xx = np.arange(len(rx.center_f_array)) * 1000.0/(rx.config.sps*rx.config.baudrate)
    x_t_s0_r10 = np.arange(len(samples[::20])) * 20 * 1000.0/sr0
    x_sense_ms = carrier_sense_array[:,0] * 1000.0/(sr0)
    fftlen = int(rx.FFTstatemx[0,0])
    masklen = int(rx.FFTstatemx[0,3])
    power_band_length = int(rx.FFTstatemx[0,8])
    carrier_sense_threshold = rx.FFTstatemx[0,6]

    """
	instr_arr[center_f_head,0] = max_expdec_corr
	instr_arr[center_f_head,1] = max_corr			# for carrier sense
	instr_arr[center_f_head,2] = max_corr_avg		# for carrier sense
	instr_arr[center_f_head,3] = max_corr_std		# for carrier sense
	# == Energy sense ====================================================================================================================================================================
	power_arr[center_f_head,0] = band_power
	power_arr[center_f_head,1] = bp_avg
	power_arr[center_f_head,2] = power_band_length * (carrier_sensed*2-1)
	"""

    fig = plt.figure(figsize=(17,13))
    ax1 = fig.add_subplot(211)
    ax2 = fig.add_subplot(212)

    ax1.plot(xx[::3], rx.center_f_array[::3], label="center f")
    ax1.plot(xx[::3], (np.abs(rx.rs_array) / np.max(np.abs(rx.rs_array)))[::3], label="sample amplitude (renornmed)")
    ax1.plot(xx[::3], rx.power_array[::3,0], label="power")
    ax1.plot(xx[::3], (rx.power_array[::3,2] > 0)*1.0, label="carrier sense")
    ax1.legend()
    ax1.grid()

    cs_criterion = (rx.fft_instr_array[:,1] - rx.fft_instr_array[:,2]) / rx.fft_instr_array[:,3]
    ax2.plot(xx[::10], rx.fft_instr_array[::10,0], label="expdec max(maskcorr)")
    ax2.plot(xx[::10], rx.fft_instr_array[::10,1], label="max(maskcorr)")
    ax2.plot(xx[::10], rx.fft_instr_array[::10,2], label="max(maskcorr) avg")
    ax2.plot(xx[::10], rx.fft_instr_array[::10,3], label="max(maskcorr) std")
    ax2.plot(xx[::10], rx.fft_instr_array[::10,4], label="carrier streak")
    ax2.plot(xx[::10], cs_criterion[::10], label="carrier sense criterion", color="black")
    ax2.legend()
    ax2.grid()

    crit_filt_y = [cs_criterion[0]]
    crit_filt_x = [xx[0]]
    for i in range(len(xx)):
        if cs_criterion[i] != crit_filt_y[-1]:
            crit_filt_y.append(cs_criterion[i])
            crit_filt_x.append(xx[i])
    crit_filt_x = np.array(crit_filt_x)
    crit_filt_y = np.array(crit_filt_y)

    fig.set_layout_engine("tight")


    fig2 = plt.figure(figsize=(17,9))
    ax3 = fig2.add_subplot(111)
    ax3.plot(xx[::10], rx.fft_instr_array[::10,2], label="max(maskcorr) avg")
    ax3.plot(xx[::10], rx.fft_instr_array[::10,3], label="max(maskcorr) std")
    ax3.plot(crit_filt_x, crit_filt_y, label="criterion")
    ax3.plot(crit_filt_x, rollsmooth(crit_filt_y, 1), label="criterion-smooth-1")
    ax3.legend()
    ax3.grid()
    fig2.set_layout_engine("tight")

    snr_arr = np.clip((rx.power_array[::3,0] - rx.power_array[::3,1]) / rx.power_array[::3,1], 1e-1, np.inf)
    snr_arr = np.log10(snr_arr) * 10.0
    fig3 = plt.figure(figsize=(17,9))
    ax5 = fig3.add_subplot(211)
    ax5.plot(xx[::3], rx.power_array[::3,0], label="power")
    ax5.plot(xx[::3], rx.power_array[::3,1], label="power avg")
    ax5.plot(xx[::3], snr_arr, label="snr")
    #ax5.plot(xx[::3], rx.dmd_array[::3], label="dmd")
    #ax5.plot(xx[::10], (rx.power_array[::10,1]*(fftlen**2))/power_band_length, label="power avg")
    #ax5.plot(xx[::10], rx.power_array[::10,2], label="power std")
    ax5.legend()
    ax5.grid()

    ax6 = fig3.add_subplot(212)
    ax6.plot(xx[:], (rx.synch_array[:,0]) % 12, label="synch...")
    ax6.legend()
    ax6.grid()

    fig3.set_layout_engine("tight")
    plt.show()






if __name__ == '__main__':
    receive_a_recording()
