import numpy as np
from matplotlib import pyplot as plt
from kuokka.lib_tools import radionoise, make_samples, gauss_curve_sps
from kuokka.lib_fft_finder import construct_fft_mask
from kuokka.lib_demodulation import demodulate, create_demod_statemx
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








def filter_demodulation_experiment(nbits, sps, baudrate, modulation_index, noiseSPD, filters, classic_demod_specs, waterfall=False, do_print=True):
    assert [(filters is None) and (classic_demod_specs is None)].count(True) == 1
    sr = baudrate * sps
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

    # Filter based demodulation
    if not (filters is None):
        filter1, filter2 = filters
        t0 = time.perf_counter()
        filtred1 = np.convolve( samples, filter1, mode="same")
        filtred2 = np.convolve( samples, filter2, mode="same")
        difference = np.abs(filtred1) - np.abs(filtred2)
        dt = time.perf_counter() - t0

    # Classic demodulation
    else:
        lp_ntaps, lp_cutoff_coeff = classic_demod_specs
        center_f_arr = np.zeros(len(samples), dtype=np.float64)
        dmd_arr = np.zeros(len(samples), dtype=np.float64)
        demodmx = create_demod_statemx(lp_ntaps=lp_ntaps, lp_cutoff_coeff=lp_cutoff_coeff, sps_f=sps)
        demodulate(sample_arr=samples, center_f_arr=center_f_arr, sample_i0=0, demod_n_samples=len(samples), dmd_arr=dmd_arr, demodmx=demodmx)
        difference = dmd_arr

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
    return ret_A, ret_B




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
