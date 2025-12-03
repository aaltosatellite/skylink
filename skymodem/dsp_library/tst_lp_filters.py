import numpy as np
from matplotlib import pyplot as  plt
import time, os, pickle
from kuokka.lib_fft_detector import construct_fft_mask, get_masklen
from kuokka.lib_tools import radionoise
from numba import njit

def compute_len_multiplier_to_E_fraction(uncut_mask, fftlen, sps, mpr_arr):
    E_all = np.sum(uncut_mask**2)
    uncut_len = len(uncut_mask)
    E_fraction_arr = np.zeros(len(mpr_arr)) * 1.0
    for i,mpr in enumerate(mpr_arr):
        masklen = int(0.5 * mpr * fftlen / sps)*2 + 1
        mask = uncut_mask[uncut_len//2-masklen//2 : uncut_len//2+masklen//2 +1]
        E_fraction_arr[i] = np.sum(mask**2) / E_all
    return E_fraction_arr



def draw_masks():
    sps = 21
    fftlen = 1024
    #masklen_g = get_masklen(fftlen=fftlen, sps=sps, mask_mode=1) + 2*200

    mask_gaussian = construct_fft_mask(sps=sps, mod_index=0.5, BT=0.5, fftlen=fftlen, masklen=fftlen-1, nn=5000)
    mask_unfiltered = construct_fft_mask(sps=sps, mod_index=0.7, BT=-1, fftlen=fftlen, masklen=fftlen-1, nn=5000)

    mpr_arr = np.linspace(0.1, 10.0, 200)
    E_fraction_arr_g = compute_len_multiplier_to_E_fraction(uncut_mask=mask_gaussian, fftlen=fftlen, sps=sps, mpr_arr=mpr_arr)
    E_fraction_arr_u = compute_len_multiplier_to_E_fraction(uncut_mask=mask_unfiltered, fftlen=fftlen, sps=sps, mpr_arr=mpr_arr)


    x = np.arange(fftlen-1) - (fftlen-1)//2
    x_mpr = (x-1)*sps*2/fftlen

    fig = plt.figure(figsize=(15,11))
    ax1 = fig.add_subplot(211)
    ax2 = fig.add_subplot(212)

    ax1.plot(x_mpr, mask_gaussian)
    ax1.plot(x_mpr, mask_unfiltered)
    ax1.grid()

    ax2.plot(mpr_arr, E_fraction_arr_g)
    ax2.plot(mpr_arr, E_fraction_arr_u)
    ax2.grid()

    fig.set_layout_engine("tight")

    plt.show()


@njit(cache=True)
def freq_shift_phased(batch, sr, fdelta, phase0):
    shifted = batch * np.exp(2j*np.pi*(fdelta/sr)*np.arange(len(batch)) + phase0*1j)
    phase1 = (phase0 + 2*np.pi*(fdelta/sr)*len(batch)) % (2*np.pi)
    return shifted, phase1

@njit(cache=True)
def fshift2_n(batch, sr, fdelta, n0):
    shifted = batch * np.exp(2j*np.pi*(fdelta/sr)*np.arange(n0, n0 + len(batch)))
    return shifted, n0+len(batch)


def speedbench_fshift():
    batch = radionoise(n=2**11+771, sr=1e6, W_per_Hz=0.2/96)

    v1, v1p    = freq_shift_phased(batch, 1e6, fdelta=-11.7e3, phase0=0.0)
    v21, v2p = freq_shift_phased(batch[:1000], 1e6, fdelta=-11.7e3, phase0=0.0)
    v22, v2p = freq_shift_phased(batch[1000:], 1e6, fdelta=-11.7e3, phase0=v2p)
    v2 = np.concatenate( (v21,v22))
    print("same: ", np.allclose(v1[:1000],v2[:1000]))
    print("same: ", np.allclose(v1,v2))


    _, _ = freq_shift_phased(batch, 1e6, -1e3, 0.0)
    _, _ = freq_shift_phased(batch, 1e6, -1e3, 0.0)
    _, _ = fshift2_n(batch, 1e6, -1.1e3, 0)
    _, _ = fshift2_n(batch, 1e6, -1.1e3, 10)

    phase = 0.0
    nn = 0

    t0 = time.perf_counter()
    for _ in range(100):
        #batch = batch * np.exp(2j*np.pi * np.arange(len(batch)) * (-3.1e3/1e6))
        #_, phase = freq_shift_phased(batch, 1e6, -1e3, phase)
        _, nn = fshift2_n(batch, 1e6, -1e3, n0=nn)
    dt = (time.perf_counter() - t0) / 100

    speed = len(batch) / dt
    cpu_fraction = dt / (len(batch)/1e6)
    print("fshift Speed:  {} MS/s".format( round(1e-6*speed, 3) ))
    print("=              {}% cpu @ 1MS/s ".format(round(100*cpu_fraction,3)))





#draw_masks()
speedbench_fshift()
