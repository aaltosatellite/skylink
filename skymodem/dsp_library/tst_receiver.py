import numpy as np
from kuokka.lib_framing import frame_packet
from kuokka.lib_reedsolomon import get_default_rs, RS_MAX_ENCODED_LEN, RS_MAX_PL_LEN, RS_MIN_ENCODED_LEN
from kuokka.lib_tools import FS1P_SYNCHWORD, FS1P_SYNCHWORD_LEN, ints_to_bits
from kuokka.lib_receiver import Receiver, RXDSPConfig, precompile_receiver
from mtools.tools_dsp import waterfall_mx
from mtools.tools_system import mpr_set
from kuokka.lib_tools import make_samples2, radionoise
import time, os
from matplotlib import pyplot as plt
from copy import deepcopy
from datetime import datetime as dtime
import pickle

"""
savior_params["sps"] 				= 21		# param ~
savior_params["JPLdecay"] 			= 32.0		# param ~
savior_params["synch_delay_mpr"] 	= 22.0		# param ~
savior_params["lp_cutoff_coeff"] 	= 0.625		# param !
savior_params["lp_ntaps"] 			= 121		# param ~
savior_params["BT"] 				= -1.0		# param !
savior_params["mod_index"] 			= 0.7		# param !
"""

def generate_test_samples(f_tune, f_center, sr0, baudrate, mod_index, BT, n_payloads, noisePpHz, T_init_silence, T_interval_array, T_end_silence):
    assert len(T_interval_array) == (n_payloads-1)
    f_ofst_nrm = (f_center - f_tune) / sr0
    preamble_bits = ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
    rs_mx, rs_cfg = get_default_rs()
    payload_istart_iend_list = list()
    n_init_samples = int(sr0 * T_init_silence)
    n_end_samples = int(sr0 * T_end_silence)
    samples = np.zeros(n_init_samples, dtype=np.complex128)

    for i_pl in range(n_payloads):
        pl = os.urandom(np.random.randint(1,RS_MAX_PL_LEN))
        pl_char_ints = np.array(bytearray(pl), dtype=np.int64)
        bits = frame_packet(pl=pl_char_ints, synchword_int=FS1P_SYNCHWORD, synchword_len=FS1P_SYNCHWORD_LEN, use_scrambler=True, use_rs=True, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=True)
        bits = np.concatenate( (preamble_bits, bits) )
        tx_sps = sr0 / baudrate
        pl_samples, _ = make_samples2(sps_f=tx_sps, bitstring=bits, f_offset=f_ofst_nrm, power=1.0, mod_index=mod_index, shaper_BT_prod=BT, n_silence_start=0, n_silence_end=0)
        payload_istart_iend_list.append( (pl, len(samples), len(samples)+len(pl_samples)) )
        samples = np.concatenate((samples, pl_samples))
        if i_pl < (n_payloads -1):
            n_interval = int(sr0 * T_interval_array[i_pl])
            samples = np.concatenate((samples, np.zeros(n_interval, dtype=np.complex128)))

    samples = np.concatenate( (samples,np.zeros(n_end_samples, dtype=np.complex128)))
    samples = samples + radionoise(n=len(samples), sr=sr0, W_per_Hz=noisePpHz)
    return samples, payload_istart_iend_list



def feed_samples_to_a_receiver(dsp_config:RXDSPConfig, samples, payload_istart_iend_list, default_batchlen, do_precompile=False, add_noise_amp=0.0):
    if do_precompile:
        precompile_receiver(dsp_config=dsp_config, do_print=False)
    rx = Receiver(config=dsp_config)
    nsamples = len(samples)
    c = 0
    pl_f_p_cursor_list = list()
    t_signal = 0.0
    t_silence = 0.0
    if add_noise_amp > 0:
        rx.set_additive_noise_amplitude(W_per_Hz=add_noise_amp)
    while c < len(samples):
        batchlen = min(default_batchlen, nsamples - c)
        batch = samples[c:c+batchlen]
        t0 = time.perf_counter()
        ret_list, carrier_sensed = rx.process_batch(batch=batch, give_bits=False)
        dt_ = time.perf_counter() - t0
        batch_contains_signal = any([(not ((c>i1) or ((c+batchlen)<i0))) for (_,i0,i1) in payload_istart_iend_list])
        if batch_contains_signal:
            t_signal += dt_
        else:
            t_silence += dt_
        c += batchlen
        for (pl, f_abs, power_tuple) in ret_list:
            pl_f_p_cursor_list.append( (pl, float(f_abs), float(power_tuple[0]), c) )
    return pl_f_p_cursor_list, rx.dt_array, t_signal, t_silence



def tgt_loop(ii, noisePpHz, dsp_config:RXDSPConfig, n_payloads, f_center_error, rel_baudrate_error, tx_mod_index_override=None):
    if tx_mod_index_override is None:
        tx_mod_index = dsp_config.mod_index
    else:
        tx_mod_index = tx_mod_index_override
    assert abs(rel_baudrate_error) < 1e-4
    T_init_silence  = 2.00 * dsp_config.fftlen_mpr * dsp_config.sps / (dsp_config.sps * dsp_config.baudrate)
    T_end_silence 	= 2.00 * dsp_config.centering_delay_mpr * dsp_config.fftlen_mpr * dsp_config.sps / (dsp_config.sps * dsp_config.baudrate)
    T_interval 		= 3e-3
    nn = [x for x in ([12,]*int(n_payloads/12) + [n_payloads%12,]) if x>0]
    n_rcvd = 0
    avg_delay_t = -1
    for n_pl_run in nn:
        samples, payload_istart_iend_list = generate_test_samples(f_tune=dsp_config.rx_tune_frequency, f_center=dsp_config.rx_f_center + f_center_error, sr0=dsp_config.rx_samplerate,
                                                                                                                          baudrate=dsp_config.baudrate * (1 + rel_baudrate_error), mod_index=tx_mod_index,
                                                                                                                          BT=dsp_config.BT_rx_match, n_payloads=n_pl_run, noisePpHz=noisePpHz, T_init_silence=T_init_silence,
                                                                                                                          T_interval_array=(T_interval,)*(n_pl_run-1), T_end_silence=T_end_silence)
        pl_f_p_cursor_list, dt_array, t_signal, t_silence = feed_samples_to_a_receiver(dsp_config=dsp_config, samples=samples, payload_istart_iend_list=payload_istart_iend_list,
                                                                                                                                                                   default_batchlen=int(0.001 * dsp_config.rx_samplerate), do_precompile=False)
        n_rcvd += len(pl_f_p_cursor_list)
        delays_s, delays_t = get_dealys(payload_istart_iend_list=payload_istart_iend_list, pl_f_p_cursor_list=pl_f_p_cursor_list, sr0=dsp_config.rx_samplerate)
        if any(delays_t > 0):
            avg_delay_t = np.average([x for x in delays_t if x > 0])
    reception_rate = n_rcvd / n_payloads
    return ii, reception_rate, avg_delay_t



def measure_curve_mpr(rx_config:RXDSPConfig, n_payloads, f_center_error, rel_baudrate_error, noiseP_array, tx_mod_index_override=None):
    reception_rate_array = np.zeros(len(noiseP_array), dtype=np.float64) -1
    delay_array = np.zeros(len(noiseP_array), dtype=np.float64) -2
    argtuples = list()
    for i_noise, noisePpHz in enumerate(noiseP_array):
        argtuples.append( (i_noise, noisePpHz, rx_config, n_payloads, f_center_error, rel_baudrate_error, tx_mod_index_override) )
    ret_list, _ = mpr_set(f=tgt_loop, argtuple_list=argtuples, ncores=7, Q_or_NS="NS", picklepack=True, verbose=False)
    for i_noise, r_rate, avg_delay_t in ret_list:
        reception_rate_array[i_noise] = r_rate
        delay_array[i_noise] = avg_delay_t
    #A = surf_integral(x_arr=noiseP_array, y_arr=reception_rate_array)
    #avg_delay = np.average( [x for x in delay_array if x > 0] ) if any(delay_array > 0) else -1
    assert np.all(reception_rate_array >= 0)
    assert np.all(delay_array > -2)
    return reception_rate_array, delay_array



def measure_execution_speed(rx_config:RXDSPConfig):
    T_init_silence_minim  = 2 * rx_config.fftlen_mpr*rx_config.sps / (rx_config.sps*rx_config.baudrate)
    assert T_init_silence_minim < 5.0
    T_init_silence = 5.0
    T_end_silence_minim 	= 4.00 * rx_config.centering_delay_mpr * rx_config.fftlen_mpr*rx_config.sps / (rx_config.sps*rx_config.baudrate)
    assert T_end_silence_minim < 0.5
    T_end_silence = 0.5
    T_interval = 5e-3
    samples, payload_istart_iend_list = generate_test_samples(f_tune=rx_config.rx_tune_frequency, f_center=rx_config.rx_f_center + 1e3, sr0=rx_config.rx_samplerate,
                                                                                                                      baudrate=rx_config.baudrate*(1+1.5e-5), mod_index=rx_config.mod_index,
                                                                                                                      BT=rx_config.BT_rx_match, n_payloads=12, noisePpHz=0.02/rx_config.baudrate, T_init_silence=T_init_silence,
                                                                                                                      T_interval_array=(T_interval,)*(8-1), T_end_silence=T_end_silence)
    pl_f_p_cursor_list, dt_array, t_signal, t_silence = feed_samples_to_a_receiver(dsp_config=rx_config, samples=samples, payload_istart_iend_list=payload_istart_iend_list,
                                                                                                                                                               default_batchlen=int(0.001 * rx_config.rx_samplerate), do_precompile=False)
    n_samples = len(samples)
    n_signal_samples = sum([i1-i0 for (f,i0,i1) in payload_istart_iend_list])
    return dt_array, t_signal, t_silence, n_samples, n_signal_samples



def get_dealys(payload_istart_iend_list, pl_f_p_cursor_list, sr0):
    pl_f_p_cursor_d = dict( [(x[0],x[1:4]) for x in pl_f_p_cursor_list] )
    delays = np.zeros(len(payload_istart_iend_list))
    for i_pl,(pl,i0,i1) in enumerate(payload_istart_iend_list):
        if pl in pl_f_p_cursor_d:
            lag_samples = pl_f_p_cursor_d[pl][2] - i1
            assert lag_samples > 0
            delays[i_pl] = lag_samples
    return delays, delays * float(1.0 / sr0)



def speed_printout(dt_array, t_total, nsamples, sr0):
    speed = nsamples / t_total
    overmatch = speed / sr0
    budget_fraction	= (1/overmatch) / 0.5
    cpu_fraction	= (1/overmatch) / 1.0
    print("="*50)
    print("speed:          {} Ms/s".format( round(1e-6 * speed, 2) ))
    print("overmatch:      {}".format( round(overmatch, 2) ))
    print("budget use:     {} %".format( round(100*budget_fraction, 2) ))
    print("cpu core use:   {} %".format( round(100*cpu_fraction, 2) ))
    for i_dt in range(len(dt_array)):
        print("\tpart {}:            {} %".format(i_dt+1, round( 100*dt_array[i_dt]/np.sum(dt_array) , 2) ))
    print("parts of total:    {} %".format( round( 100*np.sum(dt_array)/t_total , 2) ))
    print("="*50)



def random_receiver_config_from_choises(attrname_array_d:dict, rx_config_basis:RXDSPConfig):
    rx_config = deepcopy(rx_config_basis)
    for _ in range(100):
        try:
            for attrname in attrname_array_d.keys():
                assert hasattr(rx_config, attrname)
                val = attrname_array_d[attrname][np.random.randint(len(attrname_array_d[attrname]))]
                setattr(rx_config, attrname, val)
            rx_config.check_validity()
            return rx_config
        except:
            continue
    raise AssertionError("All 100 rx_configs failed validity check.")



def surf_integral(x_arr, y_arr):
    A = 0
    assert len(x_arr) > 1
    assert len(x_arr) == len(y_arr)
    for i in range(len(x_arr)-1):
        assert x_arr[i+1] > x_arr[i]
        dx = x_arr[i+1] - x_arr[i]
        A += dx * (y_arr[i] + y_arr[i+1])*0.5
    return A



def save_result(rx_config:RXDSPConfig, n_payloads, noiseP_array, reception_rate_array, A, avg_delay, dpath):
    dd = {
            "version" : 5.0,
            "ts": dtime.now().isoformat(),
            "rx_config": rx_config.__dict__,
            "n_payloads": n_payloads,
            "noiseP_array" : noiseP_array,
            "reception_rate_array": reception_rate_array,
            "A": A,
            "avg_delay": avg_delay
    }
    assert os.path.isdir(dpath)
    letters1 = "".join([chr(x) for x in np.random.randint(ord("A"), ord("Z")+1, 3)])
    letters2 = "".join([chr(x) for x in np.random.randint(ord("A"), ord("Z")+1, 3)])
    f = open(os.path.join(dpath, "kuokka-curve-result-{}-{}.pkl".format(letters1,letters2)), "wb")
    f.write(pickle.dumps(dd))
    f.close()



def load_results(dpath, minimum_version, fname_contains, mandatory_d_keys):
    assert os.path.isdir(dpath)
    dlist = os.listdir(dpath)
    results = list()
    for fname in dlist:
        fpath = os.path.join(dpath, fname)
        if not os.path.isfile(fpath):
            continue
        if False in [(x in fname) for x in fname_contains]:
            continue
        f = open(fpath, "rb")
        rd = f.read()
        f.close()
        try:
            dd = pickle.loads(rd)
            assert type(dd) == dict
            if minimum_version > 0:
                assert dd["version"] >= minimum_version
            for k in mandatory_d_keys:
                assert k in dd
            results.append(dd)
        except:
            pass
    return results



def load_top_configs(dpath, minimum_version, fname_contains, mandatory_d_keys, top_n):
    default_config = RXDSPConfig(rx_samplerate=1e6, rx_tune_frequency=437.1e6, rx_f_center=437.125e6, baudrate=9600, bufferlen=800000, batch_maxlen=1024 * 16)
    results = load_results(dpath=dpath, minimum_version=minimum_version, fname_contains=fname_contains, mandatory_d_keys=mandatory_d_keys)
    results = sorted(results, key=lambda x: x["A"], reverse=True)
    configs_dicts = list()
    res_dicts = list()
    for res in results[0:top_n]:
        assert sorted(res["rx_config"].keys()) == sorted(list(default_config.__dict__.keys()))
        config = deepcopy(default_config)
        for k in res["rx_config"].keys():
            assert hasattr(config, k)
            setattr(config, k, res["rx_config"][k])
        configs_dicts.append( [config, res] )
        res_dicts.append(res)
    return configs_dicts
# ============================================================================================================================================================================================
# ============================================================================================================================================================================================
# ============================================================================================================================================================================================




















def test_precompilation_success_rate(N):
    f_tune 		= 437.1e6
    f_center 	= 437.125e6
    sr0 		= 1e6
    baudrate	= 9600
    basic_config = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=baudrate, bufferlen=800000, batch_maxlen=1024 * 8)
    basic_config.sps 					= 21
    basic_config.centering_delay_mpr 	= 5.0
    basic_config.lp_cutoff_coeff 		= 0.63
    basic_config.JPL_halflife 			= 22
    basic_config.synch_delay_mpr		= 16.0
    # new default (but sps=21) fails at: 	36, 16, 165, 272, 45
    # new default fails at: 				-
    # old default fails at:					107, 10, 22, 34, 54, 283,
    # old default (0.02 noise) fails at:	48, 82, 7, 14, 95, 40, 61, 83, 10, 268, 5, 21
    # old default (0.005 noise) fails at:	1, 102, 68, 10, 15
    # old default (0.0001 noise) fails at:	36, 37, 1, 89, 1, 11
    for i in range(N):
        print("precompile: {}/{}".format(i+1, N))
        precompile_receiver(dsp_config=basic_config, do_print=False)





def basic_test_A():
    f_tune 		= 436.0e6
    f_center 	= 437.125e6
    sr0 		= 3.6e6
    baudrate	= 9600 * 4
    n_payloads	= 12
    rx_config = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=baudrate, bufferlen=400000, batch_maxlen=1024 * 8)
    #rx_config.mod_index = 0.7
    #rx_config.BT_rx_match = -1
    noisePpHz = 0.001/baudrate
    print("[Generating samples]")
    samples, payload_istart_iend_list = generate_test_samples(f_tune=f_tune, f_center=f_center+3.1e3, sr0=sr0, baudrate=baudrate*(1+1.5e-5), mod_index=rx_config.mod_index, BT=rx_config.BT_rx_match,
                                                                                                                      n_payloads=n_payloads, noisePpHz=noisePpHz, T_init_silence=2.0, T_interval_array=(5e-3,)*(n_payloads-1), T_end_silence=2.0)
    print("[Feeding samples]")
    pl_f_p_cursor_list, dt_array, t_signal, t_silence = feed_samples_to_a_receiver(dsp_config=rx_config, samples=samples, payload_istart_iend_list=payload_istart_iend_list, default_batchlen=1024*4, do_precompile=True, add_noise_amp=0.01)
    #pl_f_cursor_d = dict( [(x[0],x[1:3]) for x in pl_f_cursor_list] )

    print("Received {}/{} payloads.".format(len(pl_f_p_cursor_list), n_payloads))
    speed_printout(dt_array=dt_array, t_total=t_signal+t_silence, nsamples=len(samples), sr0=sr0)

    delays_s, delays_t = get_dealys(payload_istart_iend_list=payload_istart_iend_list, pl_f_p_cursor_list=pl_f_p_cursor_list, sr0=sr0)
    for i_pl,_ in enumerate(payload_istart_iend_list):
        if delays_s[i_pl] > 0:
            print("pl #{}:  lags {} ms.  ({} samples)".format(i_pl, round(1e3*float(delays_t[i_pl]), 1), delays_s[i_pl]))
        else:
            print("pl #{}:  missing".format(i_pl))
    print("\n\n")


def compare_default_optimod_4800():
    f_tune 		= 437.1e6
    f_center 	= 437.125e6
    sr0 		= 1e6
    n_payloads	= 64*3
    rx_config1 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600 // 2, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config2 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600 * 1, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config3 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600 * 2, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config4 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600 * 4, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config5 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config5.mod_index = 0.7
    rx_config5.BT_rx_match = 0.5
    rx_config6 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config6.mod_index = 0.75
    rx_config6.BT_rx_match = 0.5
    rx_config6.lp_cutoff_coeff = 0.575

    rel_noiseP_array = np.array([1e-5, 0.02, 0.04, 0.06, 0.08, 0.10, 0.12, 0.14, 0.16, 0.18, 0.19, 0.20, 0.21, 0.22, 0.23, 0.24, 0.26, 0.28, 0.30, 0.32, 0.34]) # , 0.28
    noiseP_array1 = rel_noiseP_array / rx_config1.baudrate
    noiseP_array2 = rel_noiseP_array / rx_config2.baudrate
    noiseP_array3 = rel_noiseP_array / rx_config3.baudrate
    noiseP_array4 = rel_noiseP_array / rx_config4.baudrate
    noiseP_array5 = rel_noiseP_array / rx_config5.baudrate
    noiseP_array6 = rel_noiseP_array / rx_config6.baudrate

    print("1/6")
    reception_rate_array1, _ = measure_curve_mpr(rx_config=rx_config1, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array1)
    print("2/6")
    reception_rate_array2, _ = measure_curve_mpr(rx_config=rx_config2, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array2)
    print("3/6")
    reception_rate_array3, _ = measure_curve_mpr(rx_config=rx_config3, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array3)
    print("4/6")
    reception_rate_array4, _ = measure_curve_mpr(rx_config=rx_config4, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array4)
    print("5/6")
    reception_rate_array5, _ = measure_curve_mpr(rx_config=rx_config5, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array5)
    print("6/6")
    reception_rate_array6, _ = measure_curve_mpr(rx_config=rx_config6, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array6)

    fig = plt.figure(figsize=(15,11))
    ax1 = fig.add_subplot(111)
    ax1.set_title("Reception rate")

    ax1.plot(rel_noiseP_array, reception_rate_array1, label="default @ {}".format(rx_config1.baudrate))
    ax1.plot(rel_noiseP_array, reception_rate_array2, label="default @ {}".format(rx_config2.baudrate))
    ax1.plot(rel_noiseP_array, reception_rate_array3, label="default @ {}".format(rx_config3.baudrate))
    ax1.plot(rel_noiseP_array, reception_rate_array4, label="default @ {}".format(rx_config4.baudrate))
    ax1.plot(rel_noiseP_array, reception_rate_array5, label="mod_idx=0.7, BT=0.5 @ {}".format(rx_config5.baudrate), color="red", linestyle="--")
    ax1.plot(rel_noiseP_array, reception_rate_array6, label="mod_idx=0.75, BT=0.5 @ {}".format(rx_config6.baudrate), color="blue", linestyle="--")
    ax1.set_xlabel("RELATIVE noise power per 1/baudrate")
    #ax1.semilogx()
    ax1.set_ylabel("%")
    ax1.grid()
    ax1.legend()

    fig.set_layout_engine("tight")
    plt.show()













def mod_index_matrix_comparison():
    f_tune 		= 437.1e6
    f_center 	= 437.125e6
    sr0 		= 1e6
    n_payloads	= 64*3
    rx_config1 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config2 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config1.mod_index = 0.5
    rx_config2.mod_index = 0.75

    rel_noiseP_array = np.array([1e-5, 0.02, 0.04, 0.06, 0.08, 0.10, 0.12, 0.14, 0.16, 0.18, 0.19, 0.20, 0.21, 0.22, 0.23, 0.24, 0.26, 0.28, 0.30, 0.32, 0.34]) # , 0.28
    noiseP_array1 = rel_noiseP_array / rx_config1.baudrate
    noiseP_array2 = rel_noiseP_array / rx_config2.baudrate

    print("rx-0.5 / tx-0.5")
    reception_rate_array1, _ = measure_curve_mpr(rx_config=rx_config1, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array1, tx_mod_index_override=0.50)
    print("rx-0.5 / tx-0.75")
    reception_rate_array2, _ = measure_curve_mpr(rx_config=rx_config1, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array1, tx_mod_index_override=0.75)
    print("rx-0.75 / tx-0.5")
    reception_rate_array3, _ = measure_curve_mpr(rx_config=rx_config2, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array2, tx_mod_index_override=0.50)
    print("rx-0.75 / tx-0.75")
    reception_rate_array4, _ = measure_curve_mpr(rx_config=rx_config2, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array2, tx_mod_index_override=0.75)

    fig = plt.figure(figsize=(15,11))
    ax1 = fig.add_subplot(111)
    ax1.set_title("Reception rate")

    ax1.plot(rel_noiseP_array, reception_rate_array1, label="rx-0.50 // tx-0.50")
    ax1.plot(rel_noiseP_array, reception_rate_array2, label="rx-0.50 // tx-0.75")
    ax1.plot(rel_noiseP_array, reception_rate_array3, label="rx-0.75 // tx-0.50")
    ax1.plot(rel_noiseP_array, reception_rate_array4, label="rx-0.75 // tx-0.75")
    ax1.set_xlabel("RELATIVE noise power per 1/baudrate")
    #ax1.semilogx()
    ax1.set_ylabel("%")
    ax1.grid()
    ax1.legend()

    fig.set_layout_engine("tight")
    plt.show()

















def compare_fftlens():
    f_tune 		= 437.1e6
    f_center 	= 437.125e6
    sr0 		= 1e6
    n_payloads	= 32
    rx_config1 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config2 = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=1024 * 8)
    rx_config2.fftlen = 512 + 256

    rel_noiseP_array = np.array([1e-5, 0.02, 0.04, 0.06, 0.08, 0.10, 0.12, 0.14, 0.16, 0.18, 0.19, 0.20, 0.21, 0.22, 0.23, 0.24, 0.26]) # , 0.28
    noiseP_array1 = rel_noiseP_array / 9600
    noiseP_array2 = rel_noiseP_array / 9600

    print("1/4")
    reception_rate_array1, _ = measure_curve_mpr(rx_config=rx_config1, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array1)
    print("2/4")
    reception_rate_array1_sep, _ = measure_curve_mpr(rx_config=rx_config1, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array1)
    print("3/4")
    reception_rate_array2, _ = measure_curve_mpr(rx_config=rx_config2, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array2)
    print("4/4")
    reception_rate_array2_sep, _ = measure_curve_mpr(rx_config=rx_config2, n_payloads=n_payloads, f_center_error=3e3, rel_baudrate_error=1.5e-5, noiseP_array=noiseP_array2)

    fig = plt.figure(figsize=(15,11))
    ax1 = fig.add_subplot(111)
    ax1.set_title("Reception rate")

    ax1.plot(rel_noiseP_array, reception_rate_array1, label="default", color="blue")
    ax1.plot(rel_noiseP_array, reception_rate_array1_sep, label="default", linestyle="--", color="blue")
    ax1.plot(rel_noiseP_array, reception_rate_array2, label="fftlen={}".format(rx_config2.fftlen), color="orange")
    ax1.plot(rel_noiseP_array, reception_rate_array2_sep, label="fftlen={}".format(rx_config2.fftlen), linestyle="--", color="orange")
    ax1.set_xlabel("relative noise power per 1/baudrate")
    #ax1.semilogx()
    ax1.set_ylabel("%")
    ax1.grid()
    ax1.legend()

    fig.set_layout_engine("tight")
    plt.show()


def compare_timings():
    f_tune 		= 437.1e6
    f_center 	= 437.125e6
    sr0 		= 1e6
    rx_config_default = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=1024 * 8)
    config_optim_1 = deepcopy(rx_config_default)
    config_optim_1.sps = 19
    config_optim_1.lp_cutoff_coeff = 0.583
    config_optim_1.centering_delay_mpr = 2.0
    config_optim_1.synch_delay_mpr = 16
    config_optim_1.JPL_halflife = 30

    _ = measure_execution_speed(rx_config=rx_config_default)
    dt_array, t_signal, t_silence, n_samples, n_signal_samples = measure_execution_speed(rx_config=rx_config_default)
    print("Default:  t_total: {} ms,   t_signal: {} ms".format(round( 1e3*(t_silence+t_signal), 2),  round(1e3*t_signal, 2) ))

    dt_array, t_signal, t_silence, n_samples, n_signal_samples = measure_execution_speed(rx_config=config_optim_1)
    print("Optim 1:  t_total: {} ms,   t_signal: {} ms".format(round( 1e3*(t_silence+t_signal), 2),  round(1e3*t_signal, 2) ))






def optimizer_A(t_run_min):
    f_tune 				= 437.1e6
    f_center 			= 437.125e6
    sr0 				= 1e6
    n_payloads			= 32
    #mod_index 			= 0.5
    #BT_rx_match 		= 0.425
    f_center_error 		= 3e3
    rel_baudrate_error 	= 1.5e-5

    rel_noiseP_array 	= np.array([1e-5, 0.02, 0.04, 0.06, 0.08, 0.10, 0.12, 0.14, 0.16, 0.18, 0.20, 0.22, 0.24, 0.26, 0.28])   #,0.28
    noiseP_array 		= rel_noiseP_array / 9600

    rx_config_basis = RXDSPConfig(rx_samplerate=sr0, rx_tune_frequency=f_tune, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=1024 * 16)
    rx_config_basis.mod_index = 0.75
    #rx_config_basis.BT_rx_match = BT_rx_match
    #rx_config_basis.BT_rx_match = -1

    attrname_array_d1 = {
            "mod_index" :     		[0.5, 0.75],
            "sps" : 				[8,10,12,14,16,22],
            "fftlen_mpr" : 			[int(x)   for x in np.linspace(0.50,3.0, 128)  * 52],
            "centering_delay_mpr" : [float(x) for x in np.linspace(0.15,2.0, 128) * 2.0],
            "centerf_halflife" : 	[float(x) for x in np.linspace(2,20, 128)],
            "JPL_halflife" : 		[int(x)   for x in np.linspace(0.25,2.0, 128) * 55],
            "lp_ntaps" : 			[int(x*0.5)*2+1 for x in np.linspace(0.1,2.0, 128) * 161],
            #"lp_cutoff_coeff" : 	[float(x) for x in np.linspace(0.50,2.0, 128) * 0.570],
            #"mod_index" : 			[float(x) for x in np.linspace(0.50, 1.5, 256)],
            "synch_delay_mpr" : 	[float(x) for x in np.linspace(0.25,2.0, 128) * 30.0],
    }
    attrname_array_d2 = {
            "lp_cutoff_coeff" :     [float(x) for x in np.linspace(0.50,0.65, 128) * 1.0],
            "mod_index" :     		[float(x) for x in np.linspace(0.65,0.85, 128) * 1.0],
    }
    #rx_config_basis.tx_mod_index = 4
    #rx_config_basis.mod_index = 0.7

    basisA = 0.0
    for _ in range(3):
        reception_rate_array, delay_array = measure_curve_mpr(rx_config=rx_config_basis, n_payloads=n_payloads, f_center_error=f_center_error,
                                                                                         rel_baudrate_error=rel_baudrate_error, noiseP_array=noiseP_array)
        A = surf_integral(x_arr=noiseP_array, y_arr=reception_rate_array)
        avg_delay = np.average( [x for x in delay_array if x > 0] ) if any(delay_array > 0) else -1
        print("(basis: A={},   delay={} ms)".format( round(A,8), round(avg_delay*1e3, 2)  ))
        basisA += A
    basisA = basisA / 3
    print("basis-A: {}".format(basisA))


    ii = 0
    t00 = time.monotonic()
    while (time.monotonic()-t00) < (t_run_min*60):
        rx_config = random_receiver_config_from_choises(attrname_array_d=attrname_array_d1, rx_config_basis=rx_config_basis)
        n_payloads_ = n_payloads
        reception_rate_array, delay_array = measure_curve_mpr(rx_config=rx_config, n_payloads=n_payloads_, f_center_error=f_center_error,
                                                                                 rel_baudrate_error=rel_baudrate_error, noiseP_array=noiseP_array)
        A = surf_integral(x_arr=noiseP_array, y_arr=reception_rate_array)
        avg_delay = np.average( [x for x in delay_array if x > 0] ) if any(delay_array > 0) else -1
        print("#{}  A={},   delay={} ms".format(ii+1, round(A,8),  round(avg_delay*1e3, 2)))
        save_result(rx_config=rx_config, n_payloads=n_payloads_, noiseP_array=noiseP_array, reception_rate_array=reception_rate_array, A=A, avg_delay=avg_delay, dpath="/home/elmore/datasetit/mc_results/")
        print("(saved)")
        ii += 1




def analyze_results_plot():
    results = load_results(dpath="/home/elmore/datasetit/mc_results/", minimum_version=0, fname_contains=["kuokka-curve",".pkl"], mandatory_d_keys=["n_payloads",])
    results = sorted(results, key=lambda k: k["A"], reverse=True)
    print("Loaded {} results".format(len(results)))
    results0 = results.copy()

    #results = [r for r in results if r["n_payloads"] > 32]
    results = [r for r in results if r["A"] > 0]
    #results = [r for r in results if r["A"] > 2.55e-5]


    results = [r for r in results if r["rx_config"]["BT_rx_match"] > 0.0]
    #results = [r for r in results if r["rx_config"]["mod_index"] == 0.5]
    #results = [r for r in results if r["rx_config"]["mod_index"] < 1.0]
    #results = [r for r in results if r["rx_config"]["lp_cutoff_coeff"] >= 0.55]
    #results = [r for r in results if r["rx_config"]["lp_cutoff_coeff"] <= 0.60]
    #results = [r for r in results if r["rx_config"]["mod_index"] < 0.6]

    print("{} remain after filtering.".format(len(results)))


    parameter_names = ["mod_index", "lp_cutoff_coeff", "sps", "fftlen_mpr",  "centering_delay_mpr", "centerf_halflife", "JPL_halflife", "lp_ntaps", "synch_delay_mpr"]
    for res in results[0:4]:
        print("A:       {}".format(res["A"]))
        print("delay:   {}".format(1e3*res["avg_delay"]))
        for kname in parameter_names:
            print("    {}:{} {}".format(kname, " "*(20-len(kname)) ,  res["rx_config"][kname]))
        print("\n")


    A_array0 = [d["A"] for d in results0]
    A_array = [d["A"] for d in results]
    mod_idx_array = np.array([d["rx_config"]["mod_index"] for d in results])
    version_array = np.array([d["version"] for d in results])
    BT_array = [d["rx_config"]["BT_rx_match"] for d in results]
    delay_array0 = [d["avg_delay"] for d in results0]
    delay_array = [d["avg_delay"] for d in results]
    ts_bool_array = [1.0 * (dtime.fromisoformat(r["ts"]) > dtime(year=2025, month=4, day=24, hour=0, minute=0)) for r in results]
    print("sum ts_bool_array:", sum(ts_bool_array))
    color_arr = mod_idx_array


    parameter_arrays0 = dict()
    parameter_arrays = dict()
    for parameter_name in parameter_names:
        parameter_arrays0[parameter_name] = np.array([d["rx_config"][parameter_name] for d in results0])
        parameter_arrays[parameter_name] = np.array([d["rx_config"][parameter_name] for d in results])



    #plot_params = ["lp_cutoff_coeff", "centering_delay_mpr"]
    fig1 = plt.figure(figsize=(18,14))
    ax1 = fig1.add_subplot(331)
    ax2 = fig1.add_subplot(332)
    ax3 = fig1.add_subplot(333)
    ax4 = fig1.add_subplot(334)
    ax5 = fig1.add_subplot(335)
    ax6 = fig1.add_subplot(336)
    ax7 = fig1.add_subplot(337)
    ax8 = fig1.add_subplot(338)
    ax9 = fig1.add_subplot(339)
    for i,ax in enumerate([ax1,ax2,ax3,ax4,ax5,ax6,ax7,ax8,ax9]):
        ax.scatter(parameter_arrays0[parameter_names[i]], A_array0, color="grey", marker=".", s=3)
        ax.scatter(parameter_arrays[parameter_names[i]], A_array, c=color_arr)
        ax.set_xlabel(parameter_names[i])
        ax.set_ylabel("A")
        ax.grid()
    fig1.set_layout_engine("tight")



    fig2 = plt.figure(figsize=(18,14))
    ax21 = fig2.add_subplot(331)
    ax22 = fig2.add_subplot(332)
    ax23 = fig2.add_subplot(333)
    ax24 = fig2.add_subplot(334)
    ax25 = fig2.add_subplot(335)
    ax26 = fig2.add_subplot(336)
    ax27 = fig2.add_subplot(337)
    ax28 = fig2.add_subplot(338)
    ax29 = fig2.add_subplot(339)
    for i,ax in enumerate([ax21,ax22,ax23,ax24,ax25,ax26,ax27,ax28,ax29]):
        #ax.scatter(parameter_arrays0[parameter_names[i]], delay_array0, color="grey", marker=".", s=3)
        ax.scatter(parameter_arrays[parameter_names[i]], delay_array, c=color_arr)
        ax.set_xlabel(parameter_names[i])
        ax.set_ylabel("delay")
        ax.grid()
    fig2.set_layout_engine("tight")




    fig3 = plt.figure(figsize=(12,10))
    ax31 = fig3.add_subplot(321)
    ax32 = fig3.add_subplot(322)
    ax33 = fig3.add_subplot(323)
    ax34 = fig3.add_subplot(324)
    ax35 = fig3.add_subplot(325)
    ax36 = fig3.add_subplot(326)
    pairs = set()
    for i, ax in enumerate([ax31,ax32,ax33,ax34,ax35,ax36]):
        a,b = np.random.randint(0,9,2)
        a,b = tuple(sorted( (a,b) ))
        while (a==b) or ((a,b) in pairs):
            a,b = np.random.randint(0,9,2)
            a,b = tuple(sorted( (a,b) ))
        pairs.add( (a,b) )
        ax.scatter(parameter_arrays0[parameter_names[a]], parameter_arrays0[parameter_names[b]], color="grey", marker=".", s=3)
        ax.scatter(parameter_arrays[parameter_names[a]], parameter_arrays[parameter_names[b]] , c=A_array, cmap="jet")
        ax.set_xlabel(parameter_names[a])
        ax.set_ylabel(parameter_names[b])
        ax.grid()
    fig3.set_layout_engine("tight")

    delay_array = np.array(delay_array)
    ordered_indexing = sorted(list(range(len(parameter_arrays["sps"]))), key=lambda k: delay_array[k])
    ordered_indexing = np.array(ordered_indexing, dtype=np.int64)
    print(ordered_indexing.dtype)

    xarr41_A = (parameter_arrays["fftlen_mpr"]*parameter_arrays["centering_delay_mpr"]*parameter_arrays["sps"]) / (parameter_arrays["sps"]*9600)
    xarr41_B = (parameter_arrays["synch_delay_mpr"]*parameter_arrays["sps"]) / (parameter_arrays["sps"]*9600)
    xarr41_C = (parameter_arrays["lp_ntaps"]*0.5) / (parameter_arrays["sps"]*9600)

    xarr41_A = (parameter_arrays["fftlen_mpr"]*parameter_arrays["centering_delay_mpr"]) / 9600		# fftlen * centering_delay
    xarr41_B = (parameter_arrays["synch_delay_mpr"]) / 9600
    xarr41_C = (parameter_arrays["lp_ntaps"]*0.5) / (parameter_arrays["sps"]*9600)

    xarr41 = xarr41_A + xarr41_B + xarr41_C + 0.0e-3
    #delay1 = (fftlen_mpr * sps * centering_delay_mpr) / (sps * baudrate)
    #delay2 = (synch_delay_mpr * sps) / (sps * baudrate)
    #delay3 = (lp_ntaps * 0.5) / (sps * baudrate)
    print("avg error against A:     ", 1000*np.average( np.abs( delay_array[ordered_indexing] - (xarr41_A[ordered_indexing]) ) ))
    print("avg error against A+B:   ", 1000*np.average( np.abs( delay_array[ordered_indexing] - (xarr41_A[ordered_indexing]+xarr41_B[ordered_indexing]) ) ))
    print("avg error against A+C:   ", 1000*np.average( np.abs( delay_array[ordered_indexing] - (xarr41_A[ordered_indexing]+xarr41_C[ordered_indexing]) ) ))
    print("avg error against A+B+C: ", 1000*np.average( np.abs( delay_array[ordered_indexing] - (xarr41_A[ordered_indexing]+xarr41_B[ordered_indexing]+xarr41_C[ordered_indexing]) ) ))
    print(len(ordered_indexing))
    print(len(delay_array))
    print(ordered_indexing[0:10])
    fig4 = plt.figure(figsize=(12,12))
    ax41 = fig4.add_subplot(111)
    ax41.scatter(np.arange(len(ordered_indexing)), delay_array[ordered_indexing], label="measured")
    ax41.scatter(np.arange(len(ordered_indexing)), xarr41[ordered_indexing], label="expected")
    ax41.scatter(np.arange(len(ordered_indexing)), xarr41_A[ordered_indexing], label="term A (centering delay)")
    ax41.scatter(np.arange(len(ordered_indexing)), xarr41_B[ordered_indexing], label="term B (synch delay)")
    ax41.scatter(np.arange(len(ordered_indexing)), xarr41_C[ordered_indexing], label="term C (lowpass filter delay)")
    ax41.set_xlabel("X")
    ax41.set_ylabel("Y")
    ax41.grid()
    ax41.legend()
    fig4.set_layout_engine("tight")





    plt.show()












#basic_test_A()
#basic_test_A()
#basic_test_A()

#compare_default_optimod_4800()
#mod_index_matrix_comparison()

#compare_fftlens()
#compare_timings()

analyze_results_plot()

#optimizer_A(t_run_min=30.0)

#test_precompilation_success_rate(1000)
