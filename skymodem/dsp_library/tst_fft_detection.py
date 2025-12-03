import os

import numpy as np
from matplotlib import pyplot as plt
import time
from mtools.tools_dsp import waterfall_mx
from kuokka.lib_fft_finder import create_fft_f_centerer_csense_statemx, fft_f_centerer_csense
#from kuokka.lib_fft_detector import create_fft_centering_statemx, fft_detect_and_freq_determ
from kuokka.lib_pll_detector import create_frequency_mapper_statev, pll_detect_and_freq_determ
from mtools.tools_dsp import create_pll_statevector, create_trigger_statevector, create_winstd_statemx
from kuokka.lib_tools import radionoise, make_samples2, get_frequency_search_map
from mtools.tools_math import rollsmooth
from mtools.tools_system import mpr_set




def tst_fft_center_detect_1():
    f_tune 			= 437.1e6
    f_signal 		= 437.125e6
    f_offset 		= f_signal - f_tune
    sps				= 12
    baudrate		= 9600
    sr 				= sps*baudrate
    nnoise_init		= sps * 1400 * 14
    nnoise_mid		= sps * 200
    nnoise_end		= sps * 1200
    BT = -1
    f_offset_rel1 	= f_offset / sr
    f_offset_rel2 	= f_offset / sr
    f_offset_rel3 	= f_offset / sr

    f_offset_rel	= 0.0
    mod_index 		= 0.707
    bits 			= np.random.randint(0,2, 600)*2-1
    sig_samples1,_ 	= make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_rel1, power=1.0, mod_index=mod_index, shaper_BT_prod=0.5)
    sig_samples2,_ 	= make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_rel2, power=1.0, mod_index=mod_index, shaper_BT_prod=0.5)
    sig_samples3,_ 	= make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_rel3, power=1.0, mod_index=mod_index, shaper_BT_prod=0.5)
    nsignal			= len(sig_samples1)
    noise1 			= np.zeros(nnoise_init, dtype=np.complex128)
    noise2 			= np.zeros(nnoise_mid, dtype=np.complex128)
    noise3 			= np.zeros(nnoise_end, dtype=np.complex128)
    samples0 		= np.concatenate( (noise1, sig_samples1, noise2, sig_samples2, noise2, sig_samples3, noise3) )
    samples0		= samples0
    nsamples 		= len(samples0)
    samples 		= samples0.copy()
    sig_x_arr 		= np.zeros( (3,2) )
    sig_x_arr[0]	= np.array( (nnoise_init, nnoise_init+nsignal))
    sig_x_arr[1]	= np.array( (nnoise_init+nsignal+nnoise_mid, nnoise_init+nsignal*2+nnoise_mid))
    sig_x_arr[2]	= np.array( (nnoise_init+nsignal*2+nnoise_mid*2, nnoise_init+nsignal*3+nnoise_mid*2))
    sig_y_arr		= np.zeros( (3,2) )
    sig_y_arr[0]	= np.array( (f_offset_rel1, f_offset_rel1) )
    sig_y_arr[1]	= np.array( (f_offset_rel2, f_offset_rel2) )
    sig_y_arr[2]	= np.array( (f_offset_rel3, f_offset_rel3) )
    samples 		= samples + radionoise(n=len(samples), sr=sr, W_per_Hz=0.010/9600)


    curtain_arr = np.abs( samples )
    curtain_arr = curtain_arr / np.max(curtain_arr)
    curtain_arr = rollsmooth(arr=curtain_arr, n=32, stride=1)
    #noiseamp = 1.6
    #samples = samples + np.random.normal(0, noiseamp, nsamples) + 1j*np.random.normal(0,noiseamp, nsamples)



    waterfall_mx(samples=samples, fftlen=2*sps//2, fft_jump=2*sps//4, fft_stack=2, srate=sr, plot_and_show=True, y_is_time=False)
    waterfall_mx(samples=samples, fftlen=1024*2, fft_jump=1024, fft_stack=2, srate=sr, plot_and_show=True, y_is_time=False)

    # FFT DETECTION =======================================================================================================================================
    #============================================
    fftlen				= sps * 12
    c_stat_update		= 1 / 700
    centering_delay_mpr	= 5.0
    centerf_halflife	= 12.0
    carrier_sense_threshold = 6.0
    doppler_max 	= 10928.125  # 10928.125
    #============================================
    #f_tune = 437e6
    #f_signal = 437.125e6
    #doppler_max = 10928.125  # 10928.125
    f_center_min_nrm 	= (f_signal-f_tune-doppler_max*1.2) / sr
    f_center_max_nrm 	= (f_signal-f_tune+doppler_max*1.2) / sr
    f_center_search_map	= get_frequency_search_map(fftlen=fftlen, f_min_nrm=f_center_min_nrm, f_max_nrm=f_center_max_nrm)
    #f_center_search_map = np.ones(fftlen)*1.0

    center_f_arr_fft0 	= np.zeros(nsamples, dtype=np.float64) -1
    center_f_arr_fft1 	= np.zeros(nsamples, dtype=np.float64) -1
    power_arr0			= np.zeros((nsamples,3), dtype=np.float64) -1
    power_arr1			= np.zeros((nsamples,3), dtype=np.float64) -1
    instr_arr0 			= np.zeros((nsamples,5), dtype=np.float64) -1
    instr_arr1 			= np.zeros((nsamples,5), dtype=np.float64) -1

    print("Creating statemx.")
    statemx0 = create_fft_f_centerer_csense_statemx(fftlen=fftlen, sps=sps, baudrate=baudrate, f_center_search_map=f_center_search_map,
                                                                                    mod_index=mod_index, BT_rx_match=BT, c_stat_update=c_stat_update, centering_delay_mpr=centering_delay_mpr,
                                                                                    centerf_halflife=centerf_halflife, carrier_sense_threshold=carrier_sense_threshold)

    print("Runnign fft centering and detection in one go")
    fft_f_centerer_csense(sample_arr=samples, isample0=0, nsamples=nsamples, center_f_arr=center_f_arr_fft0, power_arr=power_arr0, instr_arr=instr_arr0, center_f_head0=0, statemx=statemx0.copy())

    print("Runnign fft centering and detection in bathces")
    feed_head = 0
    statemx2 = statemx0.copy()
    while feed_head < nsamples:
        nbatch = np.random.randint(0, fftlen*2+2)
        if nbatch == 0:
            print("NBATCH=0 at",feed_head)
        #nbatch = fftlen +3
        nbatch = min(nbatch, nsamples - feed_head)
        fft_f_centerer_csense(sample_arr=samples, isample0=feed_head, nsamples=nbatch, center_f_arr=center_f_arr_fft1, power_arr=power_arr1, instr_arr=instr_arr1, center_f_head0=feed_head, statemx=statemx2)
        feed_head += nbatch

    diffs = np.abs((np.isclose(center_f_arr_fft0, center_f_arr_fft1) * 1.0)-1.0)
    differing_indexes = np.array([i for i in range(len(diffs)) if diffs[i]])

    print("outputs differ in {} points".format(sum(diffs)))
    print("such as:      ", differing_indexes[0:10])
    print("which are at: ", differing_indexes[0:10]/len(diffs))

    plt.plot(center_f_arr_fft0 - center_f_arr_fft1)
    plt.grid()
    plt.show()

    """assert np.allclose(center_f_arr_fft0, center_f_arr_fft1)

	print("Outputs of batched run and one-go run match.")

	trigger_arr_fft = center_f_arr_fft1 > -0.5
	avg_arr = instr_arr[:,0]
	var_arr = instr_arr[:,1]
	fftmax_arr = instr_arr[:,2]"""
    # FFT DETECTION =======================================================================================================================================


    # PLL DETECTION =======================================================================================================================================
    #============================================
    Z0 				= 1.81 * 0.001 * 1.0	# !!!
    c_freq 			= 0.001					# !!!
    c_phase			= c_freq**0.5			# ?
    f_limit			= 0.3					# -
    windowlen		= 300					# !
    trig_on_lvl		= Z0 * 0.7				# !!!
    trig_off_lvl	= Z0					# -
    avg_len			= sps * 8 * 4			# !
    start_margin	= sps * 8 * 3			# -
    end_margin		= sps * 8 * 3			# -
    f_upd_divisor	= 400					# -
    #============================================
    pllstatev 			= create_pll_statevector(srate=1.0, c_freq=c_freq, c_phase=c_phase, fmin=-f_limit, fmax=f_limit)
    winstd_statemx 		= create_winstd_statemx(windowlength=windowlen, avg0=0.0, std0=1.0)
    trig_statev 		= create_trigger_statevector(on_lvl=trig_on_lvl, off_lvl=trig_off_lvl, state0=0)
    cfmap_statev 		= create_frequency_mapper_statev(avg_len=avg_len, start_margin=start_margin, end_margin=end_margin, cc_update_divisor=f_upd_divisor)
    pll_f_arr 			= np.zeros(nsamples, dtype=np.float64)
    df_std_arr 			= np.zeros(nsamples, dtype=np.float64)
    trig_arr 			= np.zeros(nsamples, dtype=np.int64)
    center_f_arr_pll 	= np.zeros(nsamples, dtype=np.float64) -1

    pll_detect_and_freq_determ(rs_arr=samples, i_rs0=0, n_samples=nsamples,						pll_f_arr=pll_f_arr, pllstatev=pllstatev.copy(),
                                                       df_std_arr=df_std_arr, winstd_statemx=winstd_statemx.copy(),		trig_arr=trig_arr, trig_statev=trig_statev.copy(),
                                                       center_f_arr=center_f_arr_pll, cfmapstatev=cfmap_statev.copy())
    trigger_arr_pll = center_f_arr_pll > -0.5
    # PLL DETECTION =======================================================================================================================================


    """
		instr_arr[center_f_head,0] = max_expdec_corr
		instr_arr[center_f_head,1] = max_corr			# for carrier sense
		instr_arr[center_f_head,2] = max_corr_avg		# for carrier sense
		instr_arr[center_f_head,3] = max_corr_std		# for carrier sense
		instr_arr[center_f_head,4] = carrier_streak		# for carrier sense
	"""



    # PLOTTING ============================================================================================================================================
    waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, fft_stack=1, srate=1.0, plot_and_show=True, y_is_time=False)

    fig = plt.figure(figsize=(15,13))
    ax1 = fig.add_subplot(311)
    ax2 = fig.add_subplot(312)
    ax3 = fig.add_subplot(313)

    ax1.plot( np.arange(nsamples)[::10], (center_f_arr_fft1-f_offset_rel)[::10]*sr, label="FFT")
    ax1.plot( np.arange(nsamples)[::10], (center_f_arr_pll-f_offset_rel)[::10]*sr, label="PLL")
    for isig in range(len(sig_x_arr)):
        ax1.plot( sig_x_arr[isig], (sig_y_arr[isig]-f_offset_rel)*sr , linestyle="--", marker="x")
    ax1.grid()
    ax1.legend()

    ax2.plot( np.arange(nsamples), (power_arr0[:,2]>0)*1.0, label="FFT power[:,2]>0")
    ax2.plot( np.arange(nsamples), trigger_arr_pll, label="PLL center_f > -0.5")
    ax2.plot( np.arange(nsamples), instr_arr0[:,1] * 0.01, label="FFT max_maskcorr * 0.01")
    ax2.plot( np.arange(nsamples), curtain_arr, color="black" , label="(curtain)")
    ax2.grid()
    ax2.legend()

    ax3.plot( np.arange(nsamples), instr_arr1[:,2], label="FFT max_maskcorr avg")
    ax3.plot( np.arange(nsamples), instr_arr1[:,3], label="FFT max_maskcorr std")
    ax3.grid()
    ax3.legend()

    fig.set_layout_engine("tight")
    plt.show()
    # PLOTTING ============================================================================================================================================





def tst_fft_centering_versions(check_batchwise=False):
    f_tune 			= 437.105e6
    f_signal 		= 437.125e6
    f_offset 		= f_signal - f_tune
    sps				= 12
    baudrate		= 9600
    mod_index 		= 0.750
    nbits 			= 1760
    noise_SPD		= 0.10 / baudrate
    sr 				= sps*baudrate
    nnoise_init		= int(sr*1.5)
    nnoise_mid		= int(sps * 200)
    nnoise_end		= int(sps * 1200)
    BT 				= 0.5
    f_offset_rel1 	= f_offset / sr
    f_offset_rel2 	= f_offset / sr
    f_offset_rel3 	= f_offset / sr

    bits 			= np.random.randint(0,2, nbits)*2-1
    sig_samples1,_ 	= make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_rel1, power=1.0, mod_index=mod_index, shaper_BT_prod=0.5)
    sig_samples2,_ 	= make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_rel2, power=1.0, mod_index=mod_index, shaper_BT_prod=0.5)
    sig_samples3,_ 	= make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_rel3, power=1.0, mod_index=mod_index, shaper_BT_prod=0.5)
    nsignal1		= len(sig_samples1)
    nsignal2		= len(sig_samples2)
    nsignal3		= len(sig_samples3)
    noise1 			= np.zeros(nnoise_init, dtype=np.complex128)
    noise2 			= np.zeros(nnoise_mid, dtype=np.complex128)
    noise3 			= np.zeros(nnoise_end, dtype=np.complex128)
    samples0 		= np.concatenate( (noise1, sig_samples1, noise2, sig_samples2, noise2, sig_samples3, noise3) )
    samples0		= samples0
    nsamples 		= len(samples0)
    sig_x_arr 		= np.zeros( (3,2) , dtype=np.int64)
    sig_x_arr[0]	= np.array( (nnoise_init, nnoise_init+nsignal1))
    sig_x_arr[1]	= np.array( (sig_x_arr[0][1]+nnoise_mid, sig_x_arr[0][1]+nnoise_mid+nsignal2))
    sig_x_arr[2]	= np.array( (sig_x_arr[1][1]+nnoise_mid, sig_x_arr[1][1]+nnoise_mid+nsignal3))
    sig_y_arr		= np.zeros( (3,2) )
    sig_y_arr[0]	= np.array( (f_offset_rel1, f_offset_rel1) )
    sig_y_arr[1]	= np.array( (f_offset_rel2, f_offset_rel2) )
    sig_y_arr[2]	= np.array( (f_offset_rel3, f_offset_rel3) )
    sig_rel_offsets = np.array( (f_offset_rel1, f_offset_rel2, f_offset_rel3) )
    samples 		= samples0 + radionoise(n=len(samples0), sr=sr, W_per_Hz=noise_SPD)
    for isignal in range(len(sig_x_arr)):
        xi0, xi1 = sig_x_arr[isignal]
        assert np.average(np.abs(samples0[xi0:xi1])) > 0.99

    curtain_arr = np.abs( samples )
    curtain_arr = rollsmooth(arr=curtain_arr, n=32, stride=1)
    curtain_arr = curtain_arr / np.max(curtain_arr)
    #noiseamp = 1.6
    #samples = samples + np.random.normal(0, noiseamp, nsamples) + 1j*np.random.normal(0,noiseamp, nsamples)


    #waterfall_mx(samples=samples, fftlen=sps*52, fft_jump=2*sps//4, fft_stack=2, srate=sr, plot_and_show=True, y_is_time=False)
    waterfall_mx(samples=samples, fftlen=1024*2, fft_jump=1024, fft_stack=2, srate=sr, plot_and_show=True, y_is_time=False)

    # FFT DETECTION =======================================================================================================================================
    #============================================
    fftlen					= sps * 52
    c_stat_update			= 1 / 400
    centering_delay_mpr		= 2.0
    centerf_halflife		= 12.0
    carrier_sense_threshold = 6.0
    doppler_max 			= 10928.125  # 10928.125
    #============================================
    f_center_min_nrm 	= (f_signal-f_tune-doppler_max*1.2) / sr
    f_center_max_nrm 	= (f_signal-f_tune+doppler_max*1.2) / sr
    f_center_search_map	= get_frequency_search_map(fftlen=fftlen, f_min_nrm=f_center_min_nrm, f_max_nrm=f_center_max_nrm)



    print("Creating statemx.")
    statemx00 = create_fft_f_centerer_csense_statemx(fftlen=fftlen, sps=sps, baudrate=baudrate, f_center_search_map=f_center_search_map,
                                                                                    mod_index=mod_index, BT_rx_match=BT, c_stat_update=c_stat_update, centering_delay_mpr=centering_delay_mpr,
                                                                                    centerf_halflife=centerf_halflife, carrier_sense_threshold=carrier_sense_threshold)
    if check_batchwise:
        print("Checking one go equals batched.")
        statemx0 = statemx00.copy()
        center_f_arr_fft0 	= np.zeros(nsamples, dtype=np.float64) -1
        power_arr0			= np.zeros((nsamples,3), dtype=np.float64) -1
        instr_arr0 			= np.zeros((nsamples,5), dtype=np.float64) -1
        print("Runnign fft centering and detection in one go")
        fft_f_centerer_csense(sample_arr=samples, isample0=0, nsamples=nsamples, center_f_arr=center_f_arr_fft0, power_arr=power_arr0, instr_arr=instr_arr0, center_f_head0=0, statemx=statemx0)


        center_f_arr_fft_B 	= np.zeros(nsamples, dtype=np.float64) -1
        power_arr_B			= np.zeros((nsamples,3), dtype=np.float64) -1
        instr_arr_B 		= np.zeros((nsamples,5), dtype=np.float64) -1
        print("="*30)
        print("Runnign the same fft centering and detection in bathces")
        feed_head = 0
        statemx_B = statemx00.copy()
        while feed_head < nsamples:
            nbatch = np.random.randint(0, fftlen*2+2)
            if nbatch == 0:
                print("NBATCH=0 at",feed_head)
            #nbatch = fftlen +3
            nbatch = min(nbatch, nsamples - feed_head)
            fft_f_centerer_csense(sample_arr=samples, isample0=feed_head, nsamples=nbatch, center_f_arr=center_f_arr_fft_B, power_arr=power_arr_B, instr_arr=instr_arr_B, center_f_head0=feed_head, statemx=statemx_B)
            feed_head += nbatch
        diffs = np.abs((np.isclose(center_f_arr_fft0, center_f_arr_fft_B) * 1.0)-1.0)
        differing_indexes = np.array([i for i in range(len(diffs)) if diffs[i]])
        print("outputs differ in {} points".format(sum(diffs)))
        print("such as:      ", differing_indexes[0:10])
        print("which are at: ", differing_indexes[0:10]/len(diffs))
        print("="*30)
        print("")
        if sum(diffs) > 0:
            plt.plot(center_f_arr_fft0 - center_f_arr_fft_B)
            plt.grid()
            plt.show()
            plt.title("Differences in center_f_arr")


    masklen = int(statemx00[0,3])
    plain_masklen = int(0.5*fftlen/sps)*2 +1
    shortening = (masklen - plain_masklen)//2
    ara_ = np.arange(masklen)

    # MASK CANDIDATES =============================================================
    mask_empiric 		= statemx00[5, 0:masklen]
    mask_constant 		= np.ones(masklen, dtype=np.float64)
    mask_constant2 = mask_constant*1.0
    mask_constant2[0:shortening] = 0
    mask_constant2[-shortening:] = 0
    mask_pyramid  		= masklen//2 - np.abs(np.arange(masklen) - masklen//2)*1.0
    mask_pyramid  		= mask_pyramid / np.max(mask_pyramid)
    mask_sqrt_pyramid 	= np.sqrt(mask_pyramid)
    mask_pyramid2  		= plain_masklen//2 - np.abs(np.arange(plain_masklen) - plain_masklen//2)*1.0
    mask_pyramid2  		= mask_pyramid2 / np.max(mask_pyramid2)
    mask_pyramid2 	  	= np.concatenate( (np.zeros(shortening), mask_pyramid2, np.zeros(shortening) ) )
    mask_parable  		= ara_[-1]*ara_-ara_**2
    mask_parable  		= mask_parable / np.max(mask_parable)
    mask_sine     		= np.sin(np.linspace(0, np.pi, masklen))
    mask_sine2     		= np.sin(np.linspace(0, np.pi, plain_masklen))
    mask_sine2 	  		= np.concatenate( (np.zeros(shortening), mask_sine2, np.zeros(shortening) ) )
    mask_sinc2     		= np.sinc(np.linspace(-1, 1, plain_masklen))
    mask_sinc2 	  		= np.concatenate( (np.zeros(shortening), mask_sinc2, np.zeros(shortening) ) )
    # MASK CANDIDATES =============================================================
    #print("Const mask:",mask_constant)
    #print("Const plain mask:",mask_constant_plain)

    colors = ["blue", "orange", "green", "red", "brown", "pink", "grey"]
    linestyles = ["-", "--", ":"]

    mask_candidates = [
            [mask_empiric, "empiric"],
            [mask_constant, "constant"],
            [mask_constant2, "constant2"],
            #[mask_pyramid, "pyramid"],
            [mask_pyramid2, "pyramid2"],
            #[mask_sqrt_pyramid, "sqrt pyramid"],
            #[mask_parable, "parable"],
            [mask_sine, "sine"],
            [mask_sine2, "sine2"],
            [mask_sinc2, "sinc2"],
            #[mask_empiric + mask_sine, "empiric*1 + sine*1"],
            [mask_empiric + mask_constant2*1.2, "empiric*1 + constant2*1"],
    ]
    centerf_error_arrays = list()
    for _ in range(len(mask_candidates)):
        centerf_error_arrays.append(np.zeros(nsamples, dtype=np.float64))

    N_REPEATS = 32*4


    print("Computing single noise SPD runs.")
    mask_results = list()
    for imask, (mask, mask_name) in enumerate(mask_candidates):
        print("\tMask "+mask_name)
        cf_error_arrays, error_integral, cf_error_mono = repeat_fft_centering_run(samples=samples0, sr=sr, noise_SPD=noise_SPD,
                                        statemx00=statemx00, mask=mask, sig_idx_tuples=sig_x_arr, sig_rel_f_offsets=sig_rel_offsets, nreps=N_REPEATS)
        mask_results.append( [sig_x_arr.copy(), cf_error_arrays, error_integral, cf_error_mono, mask_name] )
        assert np.isclose(np.sum([np.sum(cf_error_mono[i0:i1]) for (i0,i1) in sig_x_arr]), error_integral)

    fig = plt.figure(figsize=(15,13))
    ax1 = fig.add_subplot(211)
    ax2 = fig.add_subplot(212)

    for imask in range(len(mask_candidates)):
        sig_idx_tuples, cf_error_arrays, error_integral, cf_error_mono, mask_name = mask_results[imask]
        color = colors[ imask % len(colors) ]
        linestyle = linestyles[ int(imask / len(colors) ) ]
        ax1.plot(np.arange(len(cf_error_mono)), cf_error_mono, label=mask_name, color=color, linestyle=linestyle)

        """for isig, cf_error_array in enumerate(cf_error_arrays):
			xarr_ = np.arange(sig_idx_tuples[isig][0],sig_idx_tuples[isig][1])
			if isig == 0:
				label = mask_name
			else:
				label = None
			ax1.plot(xarr_, cf_error_array, label=label, color=color, linestyle=linestyle)"""
        # ax1.plot( np.arange(nsamples)[::1], centerf_error_arrays[i][::1], label=mask_candidates[i][1])

        ax2.plot(np.arange(2), np.ones(2)*error_integral, label=mask_name, color=color, linestyle=linestyle)

    for isig in range(len(sig_x_arr)):
        x_ = sig_x_arr[isig][[0,0,1,1]]
        y_ = sig_y_arr[isig][[0,0,1,1]] * np.array([0,1.0,1.0,0])
        ax1.plot( x_, y_ , linestyle="--", marker="x", color="black")
    ax1.grid()
    ax1.legend()

    ax2.grid()
    ax2.legend()

    fig.set_layout_engine("tight")
    plt.show()







    print("Computing noise curves.")
    noisecurve_list = list()
    noise_SPD_arr = np.linspace(0.002/baudrate, 0.2/baudrate, 12)
    for imask, (mask, mask_name) in enumerate(mask_candidates):
        print("\t",mask_name)
        _, errint_arr = mask_SPD_curve(samples=samples0, sr=sr, noise_SPD_arr=noise_SPD_arr, statemx00=statemx00, mask=mask, sig_idx_tuples=sig_x_arr, sig_rel_f_offsets=sig_rel_offsets, nreps=160)
        noisecurve_list.append(errint_arr)

    fig = plt.figure(figsize=(15,13))
    ax1 = fig.add_subplot(111)

    for imask, (mask, mask_name) in enumerate(mask_candidates):
        #color = colors[ imask % len(colors) ]
        #linestyle = linestyles[ int(imask / len(colors) ) ]
        ax1.plot(noise_SPD_arr, noisecurve_list[imask], label=mask_name) #, color=color, linestyle=linestyle)

    ax1.grid()
    ax1.legend()

    fig.set_layout_engine("tight")
    plt.show()








def mask_SPD_curve(samples, sr, noise_SPD_arr, statemx00, mask, sig_idx_tuples, sig_rel_f_offsets, nreps):
    errint_arr = np.zeros(len(noise_SPD_arr), dtype=np.float64)
    for iSPD, noise_SPD in enumerate(noise_SPD_arr):
        ret = repeat_fft_centering_run(samples, sr, noise_SPD, statemx00, mask, sig_idx_tuples, sig_rel_f_offsets, nreps)
        cf_error_arrays, error_integral, cf_error_mono = ret
        errint_arr[iSPD] = error_integral
    return noise_SPD_arr.copy(), errint_arr



def repeat_fft_centering_run(samples, sr, noise_SPD, statemx00, mask, sig_idx_tuples, sig_rel_f_offsets, nreps):
    nsamples = len(samples)
    cf_error_mono = np.zeros(nsamples, dtype=np.float64)
    cf_error_arrays = [np.zeros(int(i1-i0),dtype=np.float64) for (i0,i1) in sig_idx_tuples]
    error_integral = 0.0
    n_inner = 12
    n_outer = int(nreps / n_inner)
    balance = nreps - (n_inner * n_outer)
    n_arr = [n_inner,]*n_outer + [balance,]*(balance>0)
    argtuple0 = [samples, sr, noise_SPD, statemx00, mask, sig_idx_tuples, sig_rel_f_offsets]
    argtuples = [argtuple0+[n,] for n in n_arr]
    print("\t\tmpr set. n_outer:{},  n_inner:{}".format(n_outer, n_inner))
    ret_list, dt_list = mpr_set(f=fft_centering_run, argtuple_list= argtuples, ncores=7, Q_or_NS="NS", picklepack=True, verbose=False)
    for cf_error_arrays_, error_integral_, cf_error_mono_ in ret_list:
        cf_error_mono += cf_error_mono_
        error_integral += error_integral_ # * (1/nreps)
        for iarr, arr in enumerate(cf_error_arrays_):
            cf_error_arrays[iarr] += arr # *(1/nreps)

    for iarr, arr in enumerate(cf_error_arrays):
        cf_error_arrays[iarr] = cf_error_arrays[iarr] *(1/nreps)
    error_integral = error_integral * (1/nreps)
    cf_error_mono = cf_error_mono * (1/nreps)
    print("\t\tmpr set done. total dt: {} s, avg dt: {} s".format( round(np.sum(dt_list), 2),  round(np.average( dt_list ), 3)))
    return cf_error_arrays, error_integral, cf_error_mono


def fft_centering_run(samples, sr, noise_SPD, statemx00, mask, sig_idx_tuples, sig_rel_f_offsets, nreps):
    masklen = len(mask)
    nsamples = len(samples)
    cf_error_arrays = [np.zeros(int(i1-i0),dtype=np.float64) for (i0,i1) in sig_idx_tuples]
    cf_error_mono = np.zeros(nsamples, dtype=np.float64)
    error_integral = 0.0
    np.random.seed( [x for x in os.urandom(32)] )
    for _ in range(nreps):
        if (not (noise_SPD is None)) and (noise_SPD > 0):
            samples_ = samples + radionoise(n=nsamples, sr=sr, W_per_Hz=noise_SPD)
        else:
            samples_ = samples.copy()
        center_f_arr_ 		= np.zeros(nsamples, dtype=np.float64)
        power_arr_			= np.zeros((nsamples,3), dtype=np.float64) -1
        instr_arr_ 			= np.zeros((nsamples,5), dtype=np.float64) -1
        statemx_ = statemx00.copy()
        statemx_[5, 0:masklen] = mask
        fft_f_centerer_csense(sample_arr=samples_, isample0=0, nsamples=nsamples, center_f_arr=center_f_arr_, power_arr=power_arr_, instr_arr=instr_arr_, center_f_head0=0, statemx=statemx_)
        for i,(i0,i1) in enumerate(sig_idx_tuples):
            cf_error_arrays[i] += np.abs(center_f_arr_[i0:i1]-sig_rel_f_offsets[i])
        cf_error_mono += np.abs(center_f_arr_-sig_rel_f_offsets[0])
    error_integral += sum( [np.sum(x) for x in cf_error_arrays] )
    return cf_error_arrays, error_integral, cf_error_mono




#tst_fft_center_detect_1()
tst_fft_centering_versions()
