import numpy as np
from numba import njit, int64
from mtools.tools_dsp import pll_cont_strm, winstd_cont_strm, trigger_vector_cont_strm, pll_cont_step, create_pll_statevector


@njit(cache=True)
def create_frequency_mapper_statev(avg_len, start_margin, end_margin, cc_update_divisor):
    for p in (avg_len,start_margin,end_margin,cc_update_divisor):
        assert p > 0
    statev = np.zeros(6, dtype=np.int64)
    statev[0] = avg_len
    statev[1] = start_margin
    statev[2] = end_margin
    statev[3] = cc_update_divisor
    return statev

@njit(cache=True)
def get_dmod_delay(fequency_map_statev):
    avg_len = fequency_map_statev[0]
    start_margin = fequency_map_statev[1]
    return avg_len + start_margin + 3


@njit(cache=True)
def map_center_frequencies(pll_f_arr, trig_arr, i_pllftrig0, nsamples, center_f_arr, statev):
    assert type(trig_arr[0]) is int64
    assert type(statev[0]) is int64
    avg_len 		= statev[0]
    start_margin 	= statev[1]  		# TODO: This is a rather critical parameter (dictated by worst case pll lock-time)
    end_margin 		= statev[2]  		# TODO: This is a rather critical parameter (dictated by synch delay btw...)
    # decoding_delay > (avg_len + start_margin) allows decoding_head to march at a constant rate. Discontinuities in frequency estimation always get resolved before it.
    cc_update		= 1.0/float(statev[3])
    previous_count = 0
    if i_pllftrig0 > 0:
        previous_count = trig_arr[i_pllftrig0-1]
    i_pllftrig = i_pllftrig0
    for _ in range(nsamples):
        trig_count = trig_arr[i_pllftrig]
        if trig_count == avg_len:
            assert (i_pllftrig-trig_count) >= 0
            f_start = max(0, i_pllftrig -trig_count -start_margin)
            center_f_arr[f_start : i_pllftrig+1] = np.average(pll_f_arr[i_pllftrig -trig_count : i_pllftrig+1])

        elif trig_count > avg_len:
            center_f_arr[i_pllftrig] = center_f_arr[i_pllftrig-1] + (pll_f_arr[i_pllftrig] - center_f_arr[i_pllftrig-1]) * cc_update

        elif (trig_count == 0) and (previous_count >= avg_len):
            assert center_f_arr[i_pllftrig-1] != 0
            center_f_arr[i_pllftrig : i_pllftrig + end_margin] = center_f_arr[i_pllftrig-1]

        previous_count = trig_count
        i_pllftrig += 1
    return i_pllftrig



@njit(cache=True)
def pll_detect_and_freq_determ(rs_arr, i_rs0, n_samples, pll_f_arr, pllstatev, df_std_arr, winstd_statemx, trig_arr, trig_statev, center_f_arr, cfmapstatev): # TODO: i_rs0 is the COMMON index to all arrays?
    i_pll_f_arr = pll_cont_strm(in_arr=rs_arr, ii0=i_rs0, nsamples=n_samples, out_arr=pll_f_arr, io0=i_rs0, statev=pllstatev)

    if i_rs0 == 0:
        df_arr = pll_f_arr[0:i_pll_f_arr] - np.roll(pll_f_arr[0:i_pll_f_arr], 1)
        df_arr[0] = df_arr[1]
    else:
        df_arr = pll_f_arr[i_rs0:i_pll_f_arr] - pll_f_arr[i_rs0 - 1:i_pll_f_arr - 1]

    i_df_std_arr = winstd_cont_strm(in_arr=df_arr, ii0=0, nsamples=n_samples, out_arr=df_std_arr, io0=i_rs0, statemx=winstd_statemx)
    i_trig_arr = trigger_vector_cont_strm(criterion_arr=df_std_arr, ii0=i_rs0, nsamples=n_samples, out_arr=trig_arr, io0=i_rs0, statev=trig_statev)
    i_centerf_arr = map_center_frequencies(pll_f_arr=pll_f_arr, trig_arr=trig_arr, i_pllftrig0=i_rs0, nsamples=n_samples, center_f_arr=center_f_arr, statev=cfmapstatev)
    assert i_pll_f_arr == i_rs0 + n_samples
    assert i_df_std_arr == i_rs0 + n_samples
    assert i_trig_arr == i_rs0 + n_samples
    assert i_centerf_arr == i_rs0 + n_samples
    return i_rs0 + n_samples





def create_pll_detection_statemx(c_freq, c_phase, fmin, fmax, std_windowlen, trig_on_lvl, trig_off_lvl, cf_avg_len, cf_start_margin, cf_end_margin, cf_c_update):
    assert std_windowlen > 7
    pll_statev 			= create_pll_statevector(srate=1.0, c_freq=c_freq, c_phase=c_phase, fmin=fmin, fmax=fmax)
    statemx 			= np.zeros( (7, std_windowlen) , dtype=np.float64)
    statemx[0,0] 		= 0.0
    statemx[1,0:7] 		= pll_statev

    statemx[2,0] 		= std_windowlen
    statemx[2,1] 		= 1.0/std_windowlen
    statemx[2,2] 		= 0
    statemx[2,3] 		= 0.0
    statemx[3,0:std_windowlen] 		= 0.0
    statemx[4,0:std_windowlen] 		= 1.0/std_windowlen

    statemx[5,0]		= trig_on_lvl
    statemx[5,1]		= trig_off_lvl
    statemx[5,2]		= 0

    statemx[6,0]		= cf_avg_len
    statemx[6,1]		= cf_start_margin
    statemx[6,2]		= cf_end_margin
    statemx[6,3]		= cf_c_update
    return statemx


@njit(cache=True)
def pll_detect_and_freq_determ_one(rs_arr, i_rs0, n_samples, pll_f_arr, center_f_arr, statemx): # TODO: i_rs0 is the COMMON index to all arrays?
    pll_f 				= statemx[0,0]								# var
    pll_statev 			= statemx[1,0:7]

    winstd_windowlen 	= int(statemx[2,0])
    winstd_cc 			= statemx[2,1]
    winstd_window_idx 	= int(statemx[2,2])							# var
    winstd_avg 			= np.sum(statemx[3,0:winstd_windowlen])		# var
    winstd_var 			= np.sum(statemx[4,0:winstd_windowlen])		# var
    winstd_avg_arr 		= statemx[3,0:winstd_windowlen]
    winstd_var_arr 		= statemx[4,0:winstd_windowlen]

    trig_on_lvl 		= statemx[5,0]
    trig_off_lvl 		= statemx[5,1]
    trig_count 			= int(statemx[5,2])							# var

    cf_avg_len 			= int(statemx[6,0])
    cf_start_margin 	= int(statemx[6,1])
    cf_end_margin 		= int(statemx[6,2])
    cf_c_update 		= statemx[6,3]
    for i in range(i_rs0, i_rs0+n_samples):
        #=================
        pll_f_prev = pll_f
        pll_f = pll_cont_step(rs_arr[i], out_arr=pll_f_arr, io0=i, statev=pll_statev)
        pll_df = pll_f - pll_f_prev
        #=================

        #=================
        avg_contr = pll_df * winstd_cc
        winstd_avg = winstd_avg + avg_contr - winstd_avg_arr[winstd_window_idx]
        var_contr = winstd_cc * ((pll_df - winstd_avg)**2)
        winstd_var = winstd_var + var_contr - winstd_var_arr[winstd_window_idx]
        winstd_avg_arr[winstd_window_idx] = avg_contr
        winstd_var_arr[winstd_window_idx] = var_contr
        winstd_window_idx = (winstd_window_idx+1) % winstd_windowlen
        df_std = winstd_var**0.5
        #center_f_arr[i] = df_std
        #=================

        #=================
        previous_count = trig_count
        if df_std < trig_on_lvl:
            trig_count += 1
        if df_std > trig_off_lvl:
            trig_count = 0

        if trig_count == cf_avg_len:
            assert (i-trig_count) >= 0
            f_start = max(0, i -trig_count -cf_start_margin)
            center_f_arr[f_start : i+1] = np.average(pll_f_arr[i-trig_count : i+1])

        elif trig_count > cf_avg_len:
            center_f_arr[i] = center_f_arr[i-1] + (pll_f_arr[i] - center_f_arr[i-1]) * cf_c_update

        elif (trig_count == 0) and (previous_count >= cf_avg_len):
            assert center_f_arr[i-1] != 0
            center_f_arr[i : i + cf_end_margin] = center_f_arr[i-1]
        #=================

    statemx[0,0] 	= pll_f
    statemx[1,0:7] 	= pll_statev
    statemx[2,2] 	= winstd_window_idx
    statemx[2,3] 	= winstd_avg
    statemx[2,4] 	= winstd_var
    statemx[5,2] 	= trig_count
    return i_rs0 + n_samples
