import time
import numpy as np
from scipy.signal import firwin
from matplotlib import pyplot as plt
from scipy.signal import windows
from kuokka.lib_resampler import create_resampler, resampler_execute, firdes_kaiser, resampler_execute_stream
from kuokka.lib_tools import radionoise
from mtools.tools_dsp import fft_usual


def tst_resampling_1():
    t_arr = np.linspace(0, 2*np.pi*10, 800)
    x_arr = np.sin(t_arr)
    x_arr = x_arr + np.cos(t_arr*2.5+1)*0.6
    x_arr = x_arr * np.cos(np.linspace(-np.pi/2, np.pi/2, 800))**3
    x_arr = x_arr * (1+0j)

    t20 = t_arr[::2]
    x20 = x_arr[::2]
    dt20 = t20[1] - t20[0]
    print("n samples to process: {}".format(len(x20)))


    fft1, freqs1 = fft_usual(iq_arr=x_arr*(1+0j),  srate=1.0,  take_abs=True)
    fft2, freqs2 = fft_usual(iq_arr=x20*(1+0j),  srate=1.0,  take_abs=True)
    fig = plt.figure(figsize=(14,12))
    ax = fig.add_subplot(111)
    ax.plot(freqs1, fft1)
    ax.plot(freqs2, fft2)
    ax.grid()
    fig.set_layout_engine("tight")
    plt.show()


    mm = 13
    n_banks = 64
    r_rate = 9600*32 / 1e6
    f_cutoff = r_rate * 0.499
    #r_rate = 1.15651
    #f_cutoff = r_rate * 0.35
    batch_len = 200
    #r_rate = 0.65
    #f_cutoff = 0.3
    print("rate factor: {}".format(r_rate))
    noise = np.random.normal(0,1, 1200) + 1j*np.random.normal(0,1,1200)


    # version 2 ------------------------------------------------------------------------------------------------------------------------------------------------
    statemx = create_resampler(m_halflen=mm, n_banks=n_banks, r_rate=r_rate, f_cutoff=f_cutoff)
    x_tst = x20[0:batch_len]
    N_timing_rep = 600
    _ = resampler_execute(samples=x20, statemx=statemx)
    _ = resampler_execute(samples=x20, statemx=statemx)
    t0 = time.perf_counter()
    for _ in range(N_timing_rep):
        _ = resampler_execute(samples=x_tst, statemx=statemx)
    dt0 = time.perf_counter() - t0
    dt2_exec = dt0 / N_timing_rep
    dt2_sample = dt2_exec / len(x_tst)
    dt2_execution_ms = 1e3 * dt2_exec
    speed2_per_sample = 1 / dt2_sample
    print("dt2 execute:  {} ms".format( round(dt2_execution_ms, 3) ))
    print("dt2 execute:  {} µs per sample".format( round(1e3 * dt2_execution_ms/len(x_tst), 3) ))
    print("Implying:     {} Msample/s".format( round(speed2_per_sample/1e6, 2) ))
    print("")
    statemx = create_resampler(m_halflen=mm, n_banks=n_banks, r_rate=r_rate, f_cutoff=f_cutoff)
    _ = resampler_execute(samples=noise, statemx=statemx)
    y_comp2 = resampler_execute(samples=noise, statemx=statemx)
    # version 2 ------------------------------------------------------------------------------------------------------------------------------------------------

    # version 3 ------------------------------------------------------------------------------------------------------------------------------------------------
    statemx = create_resampler(m_halflen=mm, n_banks=n_banks, r_rate=r_rate, f_cutoff=f_cutoff)
    out_arr = np.zeros(len(x20)*3, dtype=np.complex128)
    x_tst = x20[0:batch_len]
    N_timing_rep = 600
    _ = resampler_execute_stream(in_arr=x20, ii0=0, nsamples=len(x20), out_arr=out_arr, io0=0, statemx=statemx)
    _ = resampler_execute_stream(in_arr=x20, ii0=0, nsamples=len(x20), out_arr=out_arr, io0=0, statemx=statemx)
    t0 = time.perf_counter()
    for _ in range(N_timing_rep):
        _ = resampler_execute_stream(in_arr=x_tst, ii0=0, nsamples=batch_len, out_arr=out_arr, io0=0, statemx=statemx)
    dt0 = time.perf_counter() - t0
    dt2_exec = dt0 / N_timing_rep
    dt2_sample = dt2_exec / len(x_tst)
    dt2_execution_ms = 1e3 * dt2_exec
    speed2_per_sample = 1 / dt2_sample
    print("dt2 execute:  {} ms".format( round(dt2_execution_ms, 3) ))
    print("dt2 execute:  {} µs per sample".format( round(1e3 * dt2_execution_ms/len(x_tst), 3) ))
    print("Implying:     {} Msample/s".format( round(speed2_per_sample/1e6, 2) ))
    print("")
    statemx = create_resampler(m_halflen=mm, n_banks=n_banks, r_rate=r_rate, f_cutoff=f_cutoff)
    io1 = resampler_execute_stream(in_arr=noise, ii0=0, nsamples=len(noise), out_arr=out_arr, io0=0, statemx=statemx)
    io2 = resampler_execute_stream(in_arr=noise, ii0=0, nsamples=len(noise), out_arr=out_arr, io0=io1, statemx=statemx)
    y_comp3 = out_arr[io1:io2]
    # version 3 ------------------------------------------------------------------------------------------------------------------------------------------------

    out_arr = np.zeros(len(x20)*3, dtype=np.complex128)
    statemx 									= create_resampler(m_halflen=mm, n_banks=64, r_rate=r_rate, f_cutoff=f_cutoff)
    y2 											= resampler_execute(samples=x20, statemx=statemx)
    statemx 									= create_resampler(m_halflen=mm, n_banks=64, r_rate=r_rate, f_cutoff=f_cutoff)
    io_y3 										= resampler_execute_stream(in_arr=x20, ii0=0, nsamples=len(x20), out_arr=out_arr, io0=0, statemx=statemx)
    y3 											= out_arr[0:io_y3]
    ty2 = np.arange(len(y2)) * dt20 / r_rate
    ty2 = ty2 - dt20 * mm
    ty3 = np.arange(len(y3)) * dt20 / r_rate
    ty3 = ty3 - dt20 * mm
    print("Output: allocated for {} -vs- got {}".format( int(len(x20) / statemx[-2][1].real) + 2 , len(y2)))
    print("Output: allocated for {} -vs- got {}".format( int(len(x20) / statemx[-2][1].real) + 2 , io_y3))
    print("Outputs match #1 (2v3): {}".format( np.allclose(y2, y3) ))
    print("Outputs match #2 (2v3): {}".format( np.allclose(y_comp2, y_comp3) ))

    fig = plt.figure(figsize=(14,12))
    ax1 = fig.add_subplot(121)
    ax2 = fig.add_subplot(122)
    ax1.plot(t_arr, x_arr.real)
    ax1.scatter(t20, x20.real)
    ax1.plot(ty2, y2.real, marker="+")
    ax1.plot(ty2, y2.imag)

    ax2.plot(t_arr, x_arr.real)
    ax2.scatter(t20, x20.real)
    ax2.plot(ty3, y3.real, marker="+")
    ax2.plot(ty3, y3.imag)

    ax1.grid()
    ax2.grid()
    fig.set_layout_engine("tight")
    plt.show()





def tst_compare_windows():
    m_halflen = 13
    n_banks = 64
    f_cutoff = 0.499
    sr = 1.0
    ntaps = 2 * m_halflen * n_banks + 1

    taps_firwin = firwin(numtaps=ntaps, cutoff=f_cutoff/n_banks, fs=sr, pass_zero=True)
    taps_firwin = taps_firwin / np.max(taps_firwin)

    scipy_kaiser = windows.kaiser(M=ntaps, beta=5.65326, sym=True) * np.ones(ntaps)
    scipy_kaiser_sinc = windows.kaiser(M=ntaps, beta=5.65326, sym=True) * np.sinc(np.linspace(-f_cutoff*ntaps/n_banks, f_cutoff*ntaps/n_banks, ntaps))

    #stopband_att=60
    taps_liquid_kaiser1 = firdes_kaiser(n=ntaps, f_cutoff=f_cutoff/n_banks, stopband_att=60, frac_samp_offset=0.0)  #f_cutoff=f_cutoff/n_banks
    taps_liquid_kaiser2 = firdes_kaiser(n=ntaps, f_cutoff=f_cutoff/n_banks, stopband_att=60.1, frac_samp_offset=0.0)  #f_cutoff=f_cutoff/n_banks
    taps_liquid_kaiser3 = firdes_kaiser(n=ntaps, f_cutoff=f_cutoff/n_banks, stopband_att=59.9, frac_samp_offset=0.0)  #f_cutoff=f_cutoff/n_banks

    xx = np.arange(ntaps)

    fig = plt.figure(figsize=(14,12))
    ax1 = fig.add_subplot(311)
    ax2 = fig.add_subplot(312)
    ax3 = fig.add_subplot(313)
    ax1.plot(xx, taps_firwin, label="firwin")
    ax1.plot(xx, scipy_kaiser, label="scipy_kaiser")
    ax1.plot(xx, scipy_kaiser_sinc, label="scipy_kaiser * sinc")
    ax1.plot(xx, taps_liquid_kaiser1, label="liquid_kaiser")
    ax1.grid()
    ax1.legend()

    ax2.plot(xx, taps_firwin-taps_liquid_kaiser1, label="firwin - liquid_kaiser1")
    ax2.plot(xx, scipy_kaiser_sinc-taps_liquid_kaiser1, label="scipy_kaiser - liquid_kaiser1")
    ax2.plot(xx, scipy_kaiser_sinc-taps_liquid_kaiser2, label="scipy_kaiser - liquid_kaiser2")
    ax2.plot(xx, scipy_kaiser_sinc-taps_liquid_kaiser3, label="scipy_kaiser - liquid_kaiser3")
    ax2.grid()
    ax2.legend()

    taps_firwin = firwin(numtaps=21, cutoff=0.5 * 1/21, fs=1.0, pass_zero=True)
    taps_firwin = taps_firwin / np.max(taps_firwin)
    taps_window = windows.kaiser(M=21, beta=5.65326, sym=True)*np.ones(21)
    taps_sinc = np.sinc(np.linspace(-0.5, 0.5, 21))
    ax3.plot( taps_firwin )
    #ax3.plot( taps_window )
    ax3.plot( taps_sinc )
    #ax3.plot( taps_sinc * taps_window)
    ax3.grid()

    fig.set_layout_engine("tight")
    plt.show()




def tst_lowpass_effect():
    nsamples = int(1e5)
    samples = np.random.normal(0, 1.0, nsamples) + 1j * np.random.normal(0, 1.0, nsamples)
    samples = samples
    fmax = 0.5

    samples = samples + np.exp(2j*np.pi * np.arange(nsamples) * fmax*0.53) * 0.2
    samples = samples + np.exp(2j*np.pi * np.arange(nsamples) * -fmax*0.1) * 0.2

    mm = 17
    n_banks = 64
    r_rate = 0.5
    f_cutoff = r_rate * 0.5 * 0.9

    statemx = create_resampler(m_halflen=mm, n_banks=n_banks, r_rate=r_rate, f_cutoff=f_cutoff)
    y = resampler_execute(samples=samples, statemx=statemx)

    fft1, freqs1 = fft_usual(iq_arr=samples, srate=1.0, take_abs=True)
    fft2, freqs2 = fft_usual(iq_arr=y, srate=1.0*r_rate, take_abs=True)

    fig = plt.figure(figsize=(14,12))
    ax1 = fig.add_subplot(211)
    ax2 = fig.add_subplot(212)

    ax1.plot(freqs1, fft1)
    ax1.plot((-f_cutoff,-f_cutoff), (0, np.max(fft2)), color="black", linestyle="--")
    ax1.plot((f_cutoff,f_cutoff), (0, np.max(fft2)), color="black", linestyle="--")
    ax2.plot(freqs2, fft2)
    ax2.plot((-f_cutoff,-f_cutoff), (0, np.max(fft2)), color="black", linestyle="--")
    ax2.plot((f_cutoff,f_cutoff), (0, np.max(fft2)), color="black", linestyle="--")

    ax1.grid()
    ax2.grid()
    fig.set_layout_engine("tight")
    plt.show()





def speedbench_resampling(sr0, sps, baudrate, m_halflen, n_banks, do_print=True):
    sr 				= sps * baudrate
    r_rate 			= sr / sr0
    f_cutoff 		= r_rate * 0.499
    #n_banks  		= n_banks

    nsamples = 100000
    samples = radionoise(n=nsamples, sr=sr0, W_per_Hz=1.0)
    out_arr = np.zeros(nsamples, dtype=np.complex128)

    resampler_statemx = create_resampler(m_halflen=m_halflen, n_banks=n_banks, r_rate=r_rate, f_cutoff=f_cutoff, allow_aliasing=False)
    resampler_execute_stream(in_arr=samples, ii0=0, nsamples=nsamples//3, out_arr=out_arr, io0=0, statemx=resampler_statemx)
    resampler_execute_stream(in_arr=samples, ii0=0, nsamples=nsamples//3, out_arr=out_arr, io0=0, statemx=resampler_statemx)

    batchlen = 2048
    n_runs = 140
    t0 = time.perf_counter()
    for _ in range(n_runs):
        resampler_execute_stream(in_arr=samples, ii0=100, nsamples=batchlen, out_arr=out_arr, io0=100, statemx=resampler_statemx)
    T_call = (time.perf_counter() - t0) / n_runs
    speed = batchlen / T_call
    overmatch = speed / sr0
    core_fraction	= (1/overmatch) / 1.0
    budget_fraction	= (1/overmatch) / 0.5
    if do_print:
        print("-- resampling -------------------------------")
        print("sps:   		{}".format( round( sps, 1 ) ))
        print("baudrate:	{}".format( baudrate ))
        print("m_half:		{}".format( m_halflen ))
        print("speed:           {} Ms/s".format( round( 1e-6 * speed, 2 ) ))
        print("overmatch:       {} ".format( round( overmatch, 2 ) ))
        print("core use:        {} %".format( round( 100*core_fraction, 2 ) ))
        print("budget use:      {} %".format( round( 100*budget_fraction, 2 ) ))
        print("---------------------------------------------")
        print("")
        print("")
    return speed, overmatch, core_fraction, budget_fraction



def plot_speed_dependences():
    print("Measuring against sps...")
    sps_arr = np.arange(7, 33, dtype=np.int64)
    sps_core_fraction_arr = np.zeros(len(sps_arr), dtype=np.float64)
    for i, sps in enumerate(sps_arr):
        speed, overmatch, core_fraction, budget_fraction = speedbench_resampling(sr0=1e6, sps=sps, baudrate=9600, m_halflen=17, n_banks=64, do_print=False)
        sps_core_fraction_arr[i] = core_fraction

    print("Measuring against baudrate...")
    baudrate_arr = np.array([9600, 9600*2, 9600*4, 9600*8, 9600*16,], dtype=np.int64)
    baudrate_core_fraction_arr = np.zeros(len(baudrate_arr), dtype=np.float64)
    for i, baudrate in enumerate(baudrate_arr):
        speed, overmatch, core_fraction, budget_fraction = speedbench_resampling(sr0=1e6, sps=17, baudrate=baudrate, m_halflen=17, n_banks=64, do_print=False)
        baudrate_core_fraction_arr[i] = core_fraction

    print("Measuring against m_halflen...")
    m_arr = np.arange(9,33,2, dtype=np.int64)
    m_core_fraction_arr = np.zeros(len(m_arr), dtype=np.float64)
    for i, m in enumerate(m_arr):
        speed, overmatch, core_fraction, budget_fraction = speedbench_resampling(sr0=1e6, sps=17, baudrate=9600, m_halflen=int(m), n_banks=64, do_print=False)
        m_core_fraction_arr[i] = core_fraction

    print("Measuring against n_bank...")
    nb_arr = np.arange(16,64,2, dtype=np.int64)
    nb_core_fraction_arr = np.zeros(len(nb_arr), dtype=np.float64)
    for i, nb in enumerate(nb_arr):
        speed, overmatch, core_fraction, budget_fraction = speedbench_resampling(sr0=1e6, sps=17, baudrate=9600, m_halflen=15, n_banks=nb, do_print=False)
        nb_core_fraction_arr[i] = core_fraction


    fig = plt.figure(figsize=(14,12))
    ax1 = fig.add_subplot(221)
    ax2 = fig.add_subplot(222)
    ax3 = fig.add_subplot(223)
    ax4 = fig.add_subplot(224)

    ax1.plot(sps_arr, sps_core_fraction_arr)
    ax1.set_ylim(0, np.max(sps_core_fraction_arr)*1.1)
    ax1.set_xlabel("sps")
    ax1.set_ylabel("core %")
    ax1.grid()

    ax2.plot(baudrate_arr, baudrate_core_fraction_arr)
    ax2.set_ylim(0, np.max(baudrate_core_fraction_arr)*1.1)
    ax2.set_xlabel("baudrate")
    ax2.set_ylabel("core %")
    ax2.grid()

    ax3.plot(m_arr, m_core_fraction_arr)
    ax3.set_ylim(0, np.max(m_core_fraction_arr)*1.1)
    ax3.set_xlabel("m_halflen")
    ax3.set_ylabel("core %")
    ax3.grid()

    ax4.plot(nb_arr, nb_core_fraction_arr)
    ax4.set_ylim(0, np.max(nb_core_fraction_arr)*1.1)
    ax4.set_xlabel("n_banks")
    ax4.set_ylabel("core %")
    ax4.grid()

    fig.set_layout_engine("tight")
    plt.show()




#tst_resampling_1()
#tst_compare_windows()
#tst_lowpass_effect()
speedbench_resampling(sr0=1e6, sps=17, baudrate=9600, n_banks=64, m_halflen=17)
speedbench_resampling(sr0=1e6, sps=21, baudrate=9600, n_banks=64, m_halflen=21)
speedbench_resampling(sr0=1e6, sps=8, baudrate=9600*16, n_banks=64, m_halflen=17)

plot_speed_dependences()
