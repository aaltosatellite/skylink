import numpy as np
import os
import time
import pickle
import sys
from mtools.tools_dsp import waterfall_mx
import struct
from matplotlib import pyplot as plt
from scipy.signal import firwin
from mtools.tools_math import rollsmooth







def dual_plot_1(samples, sr0, fraction0, fraction_span, do_lp=False, fshift_for_lp=0.0):
    nsamples = len(samples)
    idx0 = int(fraction0*nsamples)
    idx1 = idx0 + int(fraction_span*nsamples)
    subset = samples[idx0:idx1]

    waterfall_mx(subset, fftlen=2048, fft_jump=2048, fft_stack=2, srate=12*9600*0+1, plot_and_show=True, y_is_time=True)

    if do_lp:
        assert abs(fshift_for_lp) < 0.5
        lp_filter = firwin(numtaps=160, cutoff=1.25* 0.540*9600/sr0, pass_zero=True)
        subset = subset * np.exp(2j*np.pi * np.arange(len(subset)) * fshift_for_lp)
        waterfall_mx(subset, fftlen=2048, fft_jump=2048, fft_stack=2, srate=12*9600*0+1, plot_and_show=True, y_is_time=True)
        subset_lp = np.convolve(subset, lp_filter)
        subset = subset_lp

    xx = np.arange(len(subset))

    fig = plt.figure(figsize=(18,13))
    ax1 = fig.add_subplot(121)
    ax2 = fig.add_subplot(122)

    ax1.plot(xx, np.abs(subset))
    ax1.plot(xx, rollsmooth(np.abs(subset), 100) )
    ax1.grid()
    ax1.semilogy()

    ax2.scatter(subset.real[::100], subset.imag[::100])
    ax2.grid()

    fig.set_layout_engine("tight")
    plt.show()



def fft_plot_1(samples, sr0, fraction0, fraction_span, fftlen, fshift, do_lp=False):
    nsamples = len(samples)
    idx0 = int(fraction0*nsamples)
    idx1 = idx0 + int(fraction_span*nsamples)
    subset = samples[idx0:idx1]
    subset = subset * np.exp(2j*np.pi * np.arange(len(subset)) * fshift)
    waterfall_mx(subset, fftlen=2048, fft_jump=2048, fft_stack=2, srate=1.0, plot_and_show=True, y_is_time=True)

    if do_lp:
        lp_filter = firwin(numtaps=160, cutoff=1.2 *0.540*9600/sr0, pass_zero=True)
        subset = np.convolve(subset, lp_filter)
        waterfall_mx(subset, fftlen=2048, fft_jump=2048, fft_stack=2, srate=12*9600*0+1, plot_and_show=True, y_is_time=True)

    fft = np.zeros(fftlen, dtype=np.float64)

    nsum = 10000
    for i in range(nsum):
        i0 = np.random.randint(0, len(subset)-fftlen-1)
        fft += np.abs( np.fft.fftshift( np.fft.fft( subset[i0:i0+fftlen]  ) ) )
    fft = fft / nsum

    xx = np.fft.fftshift( np.fft.fftfreq( fftlen, 1.0) )

    fig = plt.figure(figsize=(18,13))
    ax1 = fig.add_subplot(111)

    ax1.plot(xx, fft)
    ax1.grid()

    fig.set_layout_engine("tight")
    plt.show()












def load_ground_station_recording():
    fpath_main = "/home/elmore/datasetit/radiotallenteet/Tallinna_1/ss_the_CGHAE.bytes"
    fpath_main_pickle = "/home/elmore/datasetit/radiotallenteet/Tallinna_1/ss_the_CGHAE.pkl"
    assert os.path.isfile(fpath_main)
    f = open(fpath_main, "rb")
    rd = f.read()
    f.close()
    if not os.path.isfile(fpath_main_pickle):
        print("Loading samples from bytes")
        n_computed = len(rd) // (2*4)
        samples = np.zeros(n_computed, dtype=np.complex64)
        for i in range(n_computed):
            si = struct.unpack("f", rd[2*4*i:2*4*i +4])[0]
            sq = struct.unpack("f", rd[2*4*i+4:2*4*i +8])[0]
            samples[i] = si + 1j*sq
        print("All samples loaded from bytes.")
        print("Storing as a pickle.")
        f = open(fpath_main_pickle, "wb")
        f.write( pickle.dumps(samples) )
        f.close()
    else:
        print("Loading samples from pickle.")
        f = open(fpath_main_pickle, "rb")
        samples = pickle.loads(f.read())
        f.close()
        assert type(samples) == np.ndarray
        assert samples.dtype == np.complex64
    print("Samples loaded.")
    sr = 9600*12
    return samples, sr



def load_usrp_recording_with_satellite_antenna(file_letter):
    print("Loading usrp sat antenna recording (of the gs, sitting on the stairs).")
    fpath_stem = "/home/elmore/datasetit/radiotallenteet/Tallinna_1/Tallinna_-{}.pkl"
    valid_letters = ["X","C","S","Y","O"]
    fpath = fpath_stem.format(file_letter)
    assert os.path.isfile(fpath)
    f = open(fpath, "rb")
    dd = pickle.loads(f.read())
    f.close()
    print(dd.keys())
    # 'ts', 'sr', 'f_tune', 'samples', 'comments'
    ts = dd["ts"]
    sr = dd["sr"]
    f_tune = dd["f_tune"]
    samples = dd["samples"]
    comments = dd["comments"]
    #assert type(samples) == np.ndarray
    #assert samples.dtype == np.complex64
    print("Samples loaded. ({} M samples)".format( round(len(samples)/1e6, 1) ))
    print("samplerate: {} MS/s".format( round(sr / 1e6, 2) ))
    print("f_tune: {} MHz".format( round(f_tune / 1e6, 2) ))
    return samples, sr




samples, sr0 = load_ground_station_recording()
dual_plot_1(samples=samples, sr0=sr0, fraction0=0.2, fraction_span=0.2, do_lp=True, fshift_for_lp=-0.00)


#samples, sr0 = load_usrp_recording_with_satellite_antenna("O")		# S: fraction0=0.459, fraction_span=0.0063, fshift=-0.0238
#fft_plot_1(samples=samples, sr0=sr0, fraction0=0.0781, fraction_span=0.0032, fftlen=1024*4, fshift=-0.0238, do_lp=False)
