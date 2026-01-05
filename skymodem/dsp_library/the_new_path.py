import numpy as np
from matplotlib import pyplot as plt
from kuokka.lib_tools import radionoise, make_samples, gauss_curve_sps
from kuokka.lib_fft_finder import construct_fft_mask
from mtools.tools_dsp import waterfall_mx, fft_usual
import time
from scipy.signal import windows, firwin

def rollsmooth(arr, n):
    arr_ = arr.copy()
    for _ in range(n):
        arr_ = (np.roll(arr_, 1) + np.roll(arr_, -1) + arr_) * (1/3)
    return arr_


def surf_integral(x_arr, y_arr):
    A = 0
    assert len(x_arr) > 1
    assert len(x_arr) == len(y_arr)
    for i in range(len(x_arr)-1):
        assert x_arr[i+1] > x_arr[i]
        dx = x_arr[i+1] - x_arr[i]
        A += dx * (y_arr[i] + y_arr[i+1])*0.5
    return A






def test_centering_masks(nbits, sps, baudrate, modulation_index, noiseSPD):
    sr 			= baudrate * sps
    bits = np.random.randint(0,2, nbits)*2 -1
    signal_samples, _ = make_samples(samples_per_symbol=sps, bitstring=bits, frequency_offset=0.0, power=1.0, modulation_index=modulation_index, shaper_BT_prod=-1, n_silence_start=0, n_silence_end=0)
    nsignal = len(signal_samples)
    nnoise = nsignal//2 + int(sps*0.333)
    noise_samples = np.zeros(nnoise, dtype=np.complex128)
    samples = np.concatenate( (noise_samples, signal_samples, noise_samples) )
    nsamples = len(samples)
    samples = samples + radionoise(n=nsamples, sr=sr, W_per_Hz=noiseSPD)

    masklen = int(2.5 * 0.5 * 1024 / sps)*2 +1
    mask = construct_fft_mask(sps=sps, modulation_index=modulation_index, BT_rx_match=0.5, fftlen=1024, masklen=masklen, nn=1000)
    mask = mask - np.average(mask)
    classic_mask = np.ones( int(0.5 * 1024 / sps)*2 +1, dtype=np.float64 )

    mask_match = np.correlate( np.fft.fftshift(np.abs(np.fft.fft( samples[nnoise+10:nnoise+10+1024] ))), mask, mode="same")
    mask_match += np.correlate( np.fft.fftshift(np.abs(np.fft.fft( samples[nnoise+1024:nnoise+1024+1024] ))), mask, mode="same")
    classic_mask_match = np.correlate( np.fft.fftshift(np.abs(np.fft.fft( samples[nnoise+10:nnoise+10+1024] ))), classic_mask, mode="same")
    classic_mask_match += np.correlate( np.fft.fftshift(np.abs(np.fft.fft( samples[nnoise+1024:nnoise+1024+1024] ))), classic_mask, mode="same")

    ret_A = (nbits, sps, baudrate, modulation_index, noiseSPD, nsamples)
    ret_B = (mask, mask_match, classic_mask_match, bits, samples)
    return ret_A,ret_B



def filter_frequency_response(sps, baudrate, modulation_index, filter):
    sr = baudrate * sps
    if not (filter is None):
        assert len(filter) == sps, (len(filter), sps)
        filtr1 = filter
    else:
        filtr0 = np.sinc(np.linspace(-0.5,0.5, sps))
        filtr1 = filtr0 * np.exp(2j*np.pi * np.arange(len(filtr0)) * 0.5*modulation_index*baudrate/sr)
        filtr2 = filtr0 * np.exp(2j*np.pi * np.arange(len(filtr0)) * -0.5*modulation_index*baudrate/sr)

    filtred_A = np.convolve( radionoise(n=30000, sr=1.0, W_per_Hz=1.0), filtr1)
    filter_fresp_fft, filter_fresp_freqs = fft_usual(iq_arr=filtred_A, srate=sr, take_abs=True)
    for _ in range(40):
        filter_fresp_fft += fft_usual(iq_arr=np.convolve( radionoise(n=30000, sr=1.0, W_per_Hz=1.0), filtr1), srate=sr, take_abs=True)[0]
    filter_fresp_fft = filter_fresp_fft / 41
    return filter_fresp_freqs, filter_fresp_fft


def get_signal_frequency_profile(sps, baudrate, modulation_index, noiseSPD, nrep):
    sr 			= baudrate * sps
    nbits = int(1024*1.4 / sps) + 2
    fft_tx_profile = np.zeros(1024)
    freqs_tx_profile = np.zeros(1024)
    for ii in range(nrep):
        bits = np.random.randint(0,2, nbits)*2 -1
        samples, _ = make_samples(samples_per_symbol=sps, bitstring=bits, frequency_offset=0.0, power=1.0, modulation_index=modulation_index, shaper_BT_prod=-1)
        samples = samples + radionoise(n=len(samples), sr=sr, W_per_Hz=noiseSPD)
        i0 = np.random.randint(0,sps+1)
        fft_tx_profile_, freqs_tx_profile = fft_usual(iq_arr=samples[i0:i0+1024], srate=sr, take_abs=True)
        fft_tx_profile += fft_tx_profile_
    fft_tx_profile = fft_tx_profile / (nrep+1)
    return freqs_tx_profile, fft_tx_profile




def make_symbol_filters(sps, modulation_index, sinc_limit=0.5, ntaps=None, window=False, use_firwin=False):
    assert sinc_limit > 0
    assert modulation_index > 0
    assert sps > 0
    if ntaps is None:
        assert type(sps) == int
        ntaps = sps
    filtr0 = np.sinc(np.linspace(-sinc_limit*ntaps/sps,sinc_limit*ntaps/sps, ntaps))
    if window:
        filtr0 = filtr0 * windows.kaiser(M=sps, beta=5.65326, sym=True)
    if use_firwin:
        filtr0 = firwin(numtaps=ntaps, cutoff=sinc_limit/sps, fs=1.0, pass_zero=True)
    filtr1 = filtr0 * np.exp(2j*np.pi * np.arange(len(filtr0)) * 0.5*modulation_index/sps)  # modulation_index/sps = modulation_index*baudrate/sr
    filtr2 = filtr0 * np.exp(2j*np.pi * np.arange(len(filtr0)) * -0.5*modulation_index/sps)
    return filtr1, filtr2





def filter_demodulation_experiment(nbits, sps, baudrate, modulation_index, noiseSPD, filters, waterfall=False, do_print=True):
    sr 			= baudrate * sps
    bits = np.random.randint(0,2, nbits)*2 -1
    signal_samples, _ = make_samples(samples_per_symbol=sps, bitstring=bits, frequency_offset=0.0, power=1.0, modulation_index=modulation_index, shaper_BT_prod=0.5)
    nsignal = len(signal_samples)
    nnoise = nsignal//2 + int(sps*0.333)
    noise_samples = np.zeros(nnoise, dtype=np.complex128)
    samples = np.concatenate( (noise_samples, signal_samples, noise_samples) )
    nsamples = len(samples)
    samples = samples + radionoise(n=nsamples, sr=sr, W_per_Hz=noiseSPD)

    if waterfall:
        waterfall_mx(samples=samples, fftlen=sps, fft_jump=sps//2, fft_stack=1, srate=sr, plot_and_show=True, y_is_time=False)
        waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, fft_stack=1, srate=sr, plot_and_show=True, y_is_time=False)

    #filtr0 = np.sinc(np.linspace(-0.5,0.5, sps))  # !
    #filtr1 = filtr0 * np.exp(2j*np.pi * np.arange(len(filtr0)) *  0.5*modulation_index*baudrate/sr) # !
    #filtr2 = filtr0 * np.exp(2j*np.pi * np.arange(len(filtr0)) * -0.5*modulation_index*baudrate/sr) # !
    filter1, filter2 = filters

    t0 = time.perf_counter()
    filtred1 = np.convolve( samples, filter1, mode="same")
    filtred2 = np.convolve( samples, filter2, mode="same")
    difference = np.abs(filtred1) - np.abs(filtred2)
    dt = time.perf_counter() - t0

    difference_sign = np.sign( difference )
    difference_sign[0:nnoise] = 0
    difference_sign[nnoise+nsignal:] = 0

    measure_centers = nnoise + np.arange(len(bits))*sps +len(filter1)//2 -1
    measured_symbols = np.zeros(len(bits))
    for i in range(nbits):
        idx1 = int(measure_centers[i] - sps*0.5)
        idx2 = int(measure_centers[i] + sps*0.5)
        measured_symbols[i] = np.sign( np.sum( difference[idx1:idx2] ) )

    correlation = np.sum(measured_symbols * bits)
    match_sum = correlation + (len(bits)-correlation)//2



    """fig = plt.figure(figsize=(14,11))
	ax1 = fig.add_subplot(111)
	ax1.scatter( measure_centers,  bits )
	ax1.scatter( measure_centers,  measured_symbols, marker="x" )
	ax1.plot( np.arange(len(difference_sign)), difference_sign)
	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()"""



    if do_print:
        print("nsamples:    {}".format(nsamples))
        print("Correlation: {} / {}".format(correlation, nbits))
        print("match sum:   {} / {}".format(match_sum, nbits))
        print("        ==   {} %".format(round(100*match_sum/nbits,2)))

    ret_A = (correlation, match_sum, nbits, sps, baudrate, modulation_index, noiseSPD, nsamples, dt)
    ret_B = (bits, samples, filtred1, filtred2, difference, difference_sign, measure_centers, measured_symbols)
    return ret_A,ret_B




def measure_A(sps, baudrate, modulation_index, noise_arr, n_rep, filters):
    rateio_arr = noise_arr*0.0
    for i_noise in range(len(noise_arr)):
        for _ in range(n_rep):
            retA,retB = filter_demodulation_experiment(nbits=1000, sps=sps, baudrate=baudrate, modulation_index=modulation_index, noiseSPD=noise_arr[i_noise], filters=filters, waterfall=False, do_print=False)
            correlation, match_sum, nbits = retA[0:3]
            rateio_arr[i_noise] += ((match_sum / nbits) * 2 - 1.0)**2
        rateio_arr[i_noise] = rateio_arr[i_noise] / n_rep
    A = surf_integral(x_arr=noise_arr, y_arr=rateio_arr)
    return A, rateio_arr







def plot_results(ret):
    ret_A,ret_B = ret
    (correlation, match_sum, nbits, sps, baudrate, modulation_index, noiseSPD, nsamples, dt) = ret_A
    (bits, samples, filtred1, filtred2, difference, difference_sign, measure_centers, measured_symbols) = ret_B
    fig = plt.figure(figsize=(22,13))
    ax1 = fig.add_subplot(221)
    ax2 = fig.add_subplot(222)
    ax3 = fig.add_subplot(223)
    ax4 = fig.add_subplot(224)

    #masklen = len(mask)
    #freqs_mask = freqs_tx_profile[ 1024//2-masklen//2 : 1024//2+masklen//2 +1  ]
    #ax1.plot(freqs_tx_profile, fft_tx_profile, label="-")
    #ax1.plot(freqs_tx_profile, classic_mask_match*0.01, label="classic mask")
    #ax1.plot(freqs_tx_profile, mask_match*0.01, label="new mask")
    #ax1.plot(freqs_mask, mask*100, label="-")
    ax1.grid()
    #ax1.legend()

    #nn = len(filter_fresp_freqs)
    #match1 = 1200*np.abs(np.sinc( (filter_fresp_freqs-0.5*modulation_index*baudrate) / (baudrate)  ) )
    #match2 = 1200*np.abs(np.sinc( (filter_fresp_freqs-0.5*modulation_index*baudrate) / (baudrate)  ) + np.sinc( (filter_fresp_freqs+0.5*modulation_index*baudrate) / (baudrate)  ) )
    #ax2.plot(filter_fresp_freqs, filter_fresp_fft, label="-")
    #ax2.plot(filter_fresp_freqs, match1, label="-")
    #ax2.plot(filter_fresp_freqs, match2, label="-")
    ax2.grid()
    ax2.legend()

    ax3.plot(np.abs(filtred1), label="-")
    ax3.plot(np.abs(filtred2), label="-")
    ax3.grid()

    ax4.scatter( measure_centers,  bits )
    ax4.scatter( measure_centers,  measured_symbols, marker="x" )
    ax4.plot( np.arange(len(difference_sign)), difference_sign)
    ax4.grid()

    fig.set_layout_engine("tight")
    plt.show()

"""
sinc_limit=0.5 sps=17 ntaps=17
A:  4.6985233990293556e-05
A:  4.9020877160274606e-05
A:  5.0201173443418545e-05
A:  5.082336535274621e-05
A:  5.096889065459279e-05
A:  5.099250654000945e-05
A:  5.054095806699809e-05
A:  5.006459804095643e-05
A:  4.9563246656013254e-05
A:  4.9130287

sinc_limit=0.5 sps=17 ntaps=17+2		(window=False, use_firwin=False)
#6 MI:0.4652173913043478   A:4.925960798413825e-05
#7 MI:0.5260869565217391   A:5.091793039772726e-05
#8 MI:0.5869565217391304   A:5.211570824455492e-05
#9 MI:0.6478260869565217   A:5.246298049834279e-05
#10 MI:0.708695652173913   A:5.252921534682764e-05
#11 MI:0.7695652173913042   A:5.220282294625946e-05
#12 MI:0.8304347826086956   A:5.163936706912878e-05
#13 MI:0.8913043478260869   A:5.09870440044981e-05
#14 MI:0.9521739130434782   A:5.044614195667613e-05
#15 MI:1.0130434782608695   A:4.976536751302082e-05
#16 MI:1.0739130434782609   A:4.91680

sinc_limit=0.5 sps=17 ntaps=17+6		(window=False, use_firwin=False)
#6 MI:0.4652173913043478   A:5.052001876183711e-05
#7 MI:0.5260869565217391   A:5.2186478633996205e-05
#8 MI:0.5869565217391304   A:5.2994260653409075e-05
#9 MI:0.6478260869565217   A:5.310407978219697e-05
#10 MI:0.708695652173913   A:5.286245963541665e-05
#11 MI:0.7695652173913042   A:5.207636100260416e-05
#12 MI:0.8304347826086956   A:5.137429054214013e-05
#13 MI:0.8913043478260869   A:5.0470956409801124e-05
#14 MI:0.9521739130434782   A:4.952993036813446e-05
#15 MI:1.0130434782608695   A:4.843907776988635e-05
#16 MI:1.0739130434782609   A:4.7740569661458325e-05

sinc_limit=0.5 sps=17 ntaps=17+16		(window=False, use_firwin=False)
#6 MI:0.4652173913043478   A:3.267566089607007e-05
#7 MI:0.5260869565217391   A:3.36626171875e-05
#8 MI:0.5869565217391304   A:3.433934682765151e-05
#9 MI:0.6478260869565217   A:3.43875664950284e-05
#10 MI:0.708695652173913   A:3.403491394412878e-05
#11 MI:0.7695652173913042   A:3.3549964459043547e-05
#12 MI:0.8304347826086956   A:3.260539222301136e-05
#13 MI:0.8913043478260869   A:3.1438180338541666e-05
#14 MI:0.9521739130434782   A:3.048649458451704e-05
#15 MI:1.0130434782608695   A:2.970705456912879e-05
#16 MI:1.0739130434782609   A:2.912627207623106e-05

sinc_limit=0.5 sps=25 ntaps=25+6		(window=False, use_firwin=False)
#6 MI:0.4652173913043478   A:5.0289863310842796e-05
#7 MI:0.5260869565217391   A:5.2007011866714004e-05
#8 MI:0.5869565217391304   A:5.266271830610793e-05
#9 MI:0.6478260869565217   A:5.317535768821022e-05
#10 MI:0.708695652173913   A:5.286714494554924e-05
#11 MI:0.7695652173913042   A:5.2313228515624994e-05
#12 MI:0.8304347826086956   A:5.171675858191286e-05
#13 MI:0.8913043478260869   A:5.098487494081438e-05
#14 MI:0.9521739130434782   A:5.010166104403409e-05
#15 MI:1.0130434782608695   A:4.921909055397727e-05
#16 MI:1.0739130434782609   A:4.8445940843986725e-05

sinc_limit=0.5 sps=13 ntaps=13+4		(window=False, use_firwin=False)
#6 MI:0.4652173913043478   A:5.0824632486979166e-05
#7 MI:0.5260869565217391   A:5.262671549479166e-05
#8 MI:0.5869565217391304   A:5.328817592921401e-05
#9 MI:0.6478260869565217   A:5.3468702681107945e-05
#10 MI:0.708695652173913   A:5.316805711410983e-05
#11 MI:0.7695652173913042   A:5.269914018110795e-05
#12 MI:0.8304347826086956   A:5.202837334280302e-05
#13 MI:0.8913043478260869   A:5.09523936730587e-05
#14 MI:0.9521739130434782   A:5.017551361268938e-05
#15 MI:1.0130434782608695   A:4.930783386600377e-05
#16 MI:1.0739130434782609   A:4.846749704071969e-05

sinc_limit=0.5 sps=9 ntaps=9+4		(window=False, use_firwin=False)
#6 MI:0.4652173913043478   A:5.150375946969696e-05
#7 MI:0.5260869565217391   A:5.2993990944602266e-05
#8 MI:0.5869565217391304   A:5.349809410511362e-05
#9 MI:0.6478260869565217   A:5.371386260061554e-05  !!!
#10 MI:0.708695652173913   A:5.322896842447916e-05
#11 MI:0.7695652173913042   A:5.25217871685606e-05
#12 MI:0.8304347826086956   A:5.179586115056817e-05
#13 MI:0.8913043478260869   A:5.066946478456439e-05
#14 MI:0.9521739130434782   A:4.971919966264203e-05
#15 MI:1.0130434782608695   A:4.8675258996212115e-05
#16 MI:1.0739130434782609   A:4.797791569010416e-05


"""

def measure_curve_by_modidx():
    n_rep = 64
    noise_arr = np.linspace(0, 0.7, 12) * 1/9600
    mod_idx_arr = np.linspace(0.1, 1.5, 24)
    curves = list()
    As = list()
    for ii,modulation_index in enumerate(mod_idx_arr):
        filters = make_symbol_filters(sps=9, modulation_index=modulation_index, sinc_limit=0.5, ntaps=9+4, window=False, use_firwin=False)
        A, rateio_curve = measure_A(sps=9, baudrate=9600, modulation_index=modulation_index, noise_arr=noise_arr, n_rep=n_rep, filters=filters)
        print("#{} MI:{}   A:{}".format(ii, modulation_index, A))
        curves.append(rateio_curve)
        As.append(A)

    fig = plt.figure(figsize=(14,11))
    ax1 = fig.add_subplot(221)
    ax2 = fig.add_subplot(222)
    ax3 = fig.add_subplot(223)
    ax4 = fig.add_subplot(224)

    ax1.plot(mod_idx_arr, As, label="A")
    ax1.set_xlabel("mod index")
    ax1.set_ylabel("A")
    ax1.grid()
    ax1.legend()

    fig.set_layout_engine("tight")
    plt.show()




def measure_curve_by_sinc_limit():
    n_rep = 64
    noise_arr = np.linspace(0, 0.7, 12) * 1/9600
    sinc_limit_arr = np.linspace(0.1, 2.3, 30)
    curves = list()
    As = list()
    for ii,sinc_limit in enumerate(sinc_limit_arr):
        filters = make_symbol_filters(sps=9, modulation_index=0.5, sinc_limit=float(sinc_limit), ntaps=9+4, window=False, use_firwin=False)
        A, rateio_curve = measure_A(sps=9, baudrate=9600, modulation_index=0.5, noise_arr=noise_arr, n_rep=n_rep, filters=filters)
        print("#{} SL:{}   A:{}".format(ii, sinc_limit, A))
        curves.append(rateio_curve)
        As.append(A)

    fig = plt.figure(figsize=(14,11))
    ax1 = fig.add_subplot(221)
    ax2 = fig.add_subplot(222)
    ax3 = fig.add_subplot(223)
    ax4 = fig.add_subplot(224)

    ax1.plot(sinc_limit_arr, As, label="A")
    ax1.set_xlabel("sinc limit")
    ax1.set_ylabel("A")
    ax1.grid()
    ax1.legend()

    fig.set_layout_engine("tight")
    plt.show()




def filter_frequency_response_2(filter):
    freqs = np.linspace(-0.49, 0.49, 1000)
    response = freqs*0.0
    for i,f in enumerate(freqs):
        samples = np.exp(2j*np.pi * np.arange(3000) * f)
        response[i] = np.sum(np.abs(np.convolve(samples, filter))) / 3000.0
    return freqs, response



def plot_filter_ffts():
    filter1, _ = make_symbol_filters(sps=181, modulation_index=0.750, sinc_limit=0.1, ntaps=None, window=False, use_firwin=False)
    filter2, _ = make_symbol_filters(sps=181, modulation_index=0.750, sinc_limit=0.1, ntaps=None, window=True, use_firwin=False)
    filter3, _ = make_symbol_filters(sps=181, modulation_index=0.750, sinc_limit=0.1, ntaps=None, window=False, use_firwin=True)
    filter4 =  np.exp(2j*np.pi * np.arange(181) * +0.5*0.750/181)

    from mtools.tools_dsp import fft_usual

    fig = plt.figure(figsize=(14,11))
    ax1 = fig.add_subplot(211)
    ax2 = fig.add_subplot(212)

    fft1, freqs = fft_usual(iq_arr=filter1, srate=1.0, take_abs=True)
    fft2, freqs = fft_usual(iq_arr=filter2, srate=1.0, take_abs=True)
    fft3, freqs = fft_usual(iq_arr=filter3, srate=1.0, take_abs=True)
    fft4, freqs = fft_usual(iq_arr=filter4, srate=1.0, take_abs=True)

    ax1.plot(freqs, fft1, label="1")
    ax1.plot(freqs, fft2, label="2")
    ax1.plot(freqs, fft3, label="3")
    ax1.plot(freqs, fft4, label="4")
    ax1.grid()
    ax1.legend()

    freqs_tx_profile, fft_tx_profile = get_signal_frequency_profile(sps=17, baudrate=9600, modulation_index=0.75, noiseSPD=0.0/9600, nrep=100)

    fil0 = None
    fil0 = make_symbol_filters(sps=17, modulation_index=0.75, sinc_limit=0.5, ntaps=None, window=False, use_firwin=False)[0]
    fil1 = make_symbol_filters(sps=17, modulation_index=0.75, sinc_limit=0.1, ntaps=None, window=False, use_firwin=False)[0]
    fil2 = make_symbol_filters(sps=17, modulation_index=0.75, sinc_limit=0.1, ntaps=None, window=True, use_firwin=False)[0]
    fil3 = make_symbol_filters(sps=17, modulation_index=0.75, sinc_limit=0.1, ntaps=None, window=False, use_firwin=True)[0]
    fil4 = np.exp(2j*np.pi * np.arange(17) * +0.5*0.750/17)
    filter_fresp_freqs, filter_fresp_fft0 = filter_frequency_response_2(filter=fil0)
    filter_fresp_freqs, filter_fresp_fft1 = filter_frequency_response_2(filter=fil1)
    filter_fresp_freqs, filter_fresp_fft2 = filter_frequency_response_2(filter=fil2)
    filter_fresp_freqs, filter_fresp_fft3 = filter_frequency_response_2(filter=fil3)
    filter_fresp_freqs, filter_fresp_fft4 = filter_frequency_response_2(filter=fil4)
    filter_fresp_freqs2, filter_fresp_fft4_2 = filter_frequency_response_2(filter=fil4)

    ax2.plot(filter_fresp_freqs, filter_fresp_fft0, label="filter freq response 0")
    ax2.plot(filter_fresp_freqs, filter_fresp_fft1, label="filter freq response 1")
    ax2.plot(filter_fresp_freqs, filter_fresp_fft2, label="filter freq response 2")
    ax2.plot(filter_fresp_freqs, filter_fresp_fft3, label="filter freq response 3")
    ax2.plot(filter_fresp_freqs, filter_fresp_fft4, label="filter freq response 4")
    #ax2.plot(filter_fresp_freqs2, filter_fresp_fft4_2*16, label="filter freq response 4 (algo 2)", color="black")
    ax2.plot(freqs_tx_profile/(17*9600), fft_tx_profile/10, label="signal freq profile", color="black")
    ax2.grid()
    ax2.legend()


    fig.set_layout_engine("tight")
    plt.show()











plot_filter_ffts()

#measure_curve_by_sinc_limit()



#filters = make_symbol_filters(sps=17, modulation_index=0.705, sinc_limit=0.5, ntaps=None, window=False, use_firwin=False)
filter1 = np.exp(2j*np.pi * np.arange(17) * +0.5*0.75/17)
filter2 = np.exp(2j*np.pi * np.arange(17) * -0.5*0.75/17)
filters = (filter1,filter2)
ret_ = filter_demodulation_experiment(nbits=1000, sps=17, baudrate=9600, modulation_index=0.75, noiseSPD=0.2/9600, filters=filters, waterfall=True)
plot_results(ret_)
