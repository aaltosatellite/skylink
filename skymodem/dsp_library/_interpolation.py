import numpy as np
from numba import njit
from matplotlib import pyplot as plt
import time
from kuokka.lib_tools import make_samples2, radionoise
from kuokka.lib_resampler import resampler_execute, create_resampler, create_upsample_taps, upsample, linear_interp, cubic_interp
from scipy.signal import firwin

RESAMP_STATE_BOUNDARY = 1.0
RESAMP_STATE_INTERP   = 2.0





"""
resampling rate		OM_old_resampler		OM_upsampler
x0.51				

x2:					268						101   /  143(9,12)
x4:					155						94.0  /  127
x8:					82.5					58.2  /  78
x8.333:				81.0					52.1  /  
x16:				43.57					29.0  /  47 !		
x16.71:				42.45					????  /  47.2 !		
x32:				22.6					21.2  /  37.7		
x64:				11.5					17.4	!	
x128:				5.6(breaks)				9.29	
x208.33333			3.5(breaks)				7.0 / 10.4(10,13) / 10.6(10,12) / 10.9(9,12) / 10.1(9,16) /11.2(9,12)
x256:				2.5(breaks)				5.78	 / 9.2
x512:				1.33					3.26		
x802:				0.869					2.1	
"""




def tst1():
    nsamples 	= 1024*16
    sr0 		= 9600*4
    sr1 		= sr0 * (32.0)  # 64.12:no   64.12+32:yes   64.12+16:yes   64.12+8:yes   64.12+4:yes(less)   64.12+2:no
    ratio 		= sr1 / sr0
    phase0		= np.pi*2 * 0.3451211

    noise_ 		= radionoise(nsamples*16*2, sr=sr0*16, W_per_Hz=1.0)
    y0 			= np.convolve(noise_, firwin(201, cutoff=0.15*sr0, fs=sr0*16, pass_zero=True))
    y0			= y0[500:500+nsamples*16] # info maxf = 0.15*sr0, sr=sr0*16
    x0 			= np.arange(len(y0)) * (1/16.0)

    rsmx0 		= create_resampler(m_halflen=32, n_banks=64, r_rate=(1/16), f_cutoff=(1/16)*0.499, allow_aliasing=False)
    y1 			= resampler_execute(samples=y0, statemx=rsmx0)
    x1			= np.arange(len(y1)) * 1.0 # info maxf = 0.15*sr0, sr=sr0
    delay1 		= (32-1)/16

    print("Base ratio: ", ratio)
    mhalf = 4
    rsmx = create_resampler(m_halflen=mhalf, n_banks=max(64, int(ratio/2)*2+2), r_rate=ratio, f_cutoff=min(0.499, 0.499*ratio), allow_aliasing=False)
    _ = resampler_execute(samples=y1, statemx=rsmx)
    _ = resampler_execute(samples=y1, statemx=rsmx)
    _ = resampler_execute(samples=y1, statemx=rsmx)
    _ = resampler_execute(samples=y1, statemx=rsmx)
    t00 = time.perf_counter()
    y_rs = resampler_execute(samples=y1, statemx=rsmx)
    dt = time.perf_counter() - t00
    speed1 = len(y1) / dt
    speed2 = len(y_rs) / dt
    print("="*42)
    print("dt resample:     {} ms".format( round(1e3*dt,2) ))
    print("speed1 resample: {} MS/s".format(round(1e-6 * speed1, 3)))
    print("speed2 resample: {} MS/s".format(round(1e-6 * speed2, 3)))
    print("overmatch1:      {} ".format(round(speed1/sr0, 3)))
    print("overmatch2:      {} ".format(round(speed2/sr1, 3)))
    print("="*42)
    print("")


    scalar_arr2, taps_up2, window_ym1_2, delay_estimate2  = create_upsample_taps(r_rate=ratio, dtype=y1.dtype, m_halflen=9, L_limit=12, up_cutoff=0.499)
    _ = upsample(samples=y1, scalar_arr=scalar_arr2, taps_up=taps_up2, window_ym1=window_ym1_2)
    _ = upsample(samples=y1, scalar_arr=scalar_arr2, taps_up=taps_up2, window_ym1=window_ym1_2)
    _ = upsample(samples=y1, scalar_arr=scalar_arr2, taps_up=taps_up2, window_ym1=window_ym1_2)
    _ = upsample(samples=y1, scalar_arr=scalar_arr2, taps_up=taps_up2, window_ym1=window_ym1_2)
    t00 = time.perf_counter()
    y_upsample2 = upsample(samples=y1, scalar_arr=scalar_arr2, taps_up=taps_up2, window_ym1=window_ym1_2)
    dt = time.perf_counter() - t00
    speed1 = len(y1) / dt
    speed2 = len(y_upsample2) / dt
    print("="*42)
    print("(L_up: {})".format( int(scalar_arr2[0]) ))
    print("(r_fine: {})".format(scalar_arr2[1]))
    print("dt upsample:     {} ms".format( round(1e3*dt,2) ))
    print("speed1 upsample: {} MS/s".format(round(1e-6 * speed1, 3)))
    print("speed2 upsample: {} MS/s".format(round(1e-6 * speed2, 3)))
    print("overmatch1:        {} ".format(round(speed1/sr0, 3)))
    print("overmatch2:        {} ".format(round(speed2/sr1, 3)))
    print("="*42)
    print("")


    y_interp, _, _, _ = cubic_interp(y=y1, rate=ratio, x_minus1=0.0, y_minus1=0.0, y_minus2=0.0)
    y_interp, _, _, _ = cubic_interp(y=y1, rate=ratio, x_minus1=0.0, y_minus1=0.0, y_minus2=0.0)
    y_interp, _, _, _ = cubic_interp(y=y1, rate=ratio, x_minus1=0.0, y_minus1=0.0, y_minus2=0.0)
    y_interp, _, _, _ = cubic_interp(y=y1, rate=ratio, x_minus1=0.0, y_minus1=0.0, y_minus2=0.0)
    t00 = time.perf_counter()
    y_interp, _, _, _ = cubic_interp(y=y1, rate=ratio, x_minus1=0.0, y_minus1=0.0, y_minus2=0.0)
    dt = time.perf_counter() - t00
    speed1 = len(y1) / dt
    speed2 = len(y_interp) / dt
    print("="*42)
    print("dt interpolate:     {} ms".format( round(1e3*dt,2) ))
    print("speed1 interpolate: {} MS/s".format(round(1e-6 * speed1, 2)))
    print("speed2 interpolate: {} MS/s".format(round(1e-6 * speed2, 2)))
    print("overmatch1:      {} ".format(round(speed1/sr0, 3)))
    print("overmatch2:      {} ".format(round(speed2/sr1, 3)))
    print("="*42)
    print("")



    len_interp = int(1024*ratio)
    fig = plt.figure(figsize=(17,12))
    ax1 = fig.add_subplot(111)

    ax1.plot(x1[:1024] -delay1, 										y1[:1024].real, label="")
    #ax1.plot((np.arange(len_interp)-ratio)*1.0/ratio, 				y_interp[:len_interp].real, label="")
    ax1.plot((np.arange(len_interp)-ratio)*1.0/ratio -(mhalf-1) -delay1, 		y_rs[:len_interp].real, label="old resampler")
    ax1.plot((np.arange(len_interp)-ratio)*1.0/ratio -delay_estimate2 -delay1, 		1.0*y_upsample2[:len_interp].real, label="upsample")
    ax1.plot(x0[:1024*10], 											y0[:1024*10], color="black", linestyle="--", label="reference (y0)")
    #ax1.scatter((np.arange(len_interp)-ratio)*1.0/ratio, 					linintrp[:len_interp].real, marker="x")
    ax1.grid()
    ax1.legend()
    fig.set_layout_engine("tight")
    plt.show()







@njit(cache=True)
def sparse_conv(samples, L_up, taps_up):
    """for n in range(int(len(y_up2)/L_up-L_up)):
            for j in range(L_up):
                    kloop = int((ntaps-1-j)/L_up)+1
                    kmax = kloop-1
                    imax = j+kmax*L_up
                    inext = j+(kmax+1)*L_up
                    if (imax < ntaps) and (inext >= ntaps):
                            a+= 1
                    else:
                            b += 1
                    for k in range(kloop):
                            #assert (j+k*L_up) < ntaps
                            y_up2[n*L_up+j] += samples[n-k] * taps_up[j+k*L_up]"""
    y_up1 = np.zeros(len(samples)*L_up, dtype=samples.dtype)
    y_up2 = np.zeros(len(samples)*L_up, dtype=samples.dtype)

    for i in range(len(samples)):
        y_up1[i*L_up] = samples[i]
    y_up1 = np.convolve(y_up1, taps_up, mode="same")

    a = 0
    b = 0
    ntaps = len(taps_up)
    window = np.zeros(int((ntaps-1-0)/L_up)+1, dtype=samples.dtype)
    window_head = -1
    window_len = len(window)
    for n in range(len(samples)):
        window_head = (window_head+1)%window_len
        window[window_head] = samples[n]
        for j in range(L_up):
            kloop = int((ntaps-1-j)/L_up)+1
            kmax = kloop-1
            imax = j+kmax*L_up
            inext = j+(kmax+1)*L_up
            if (imax < ntaps) and (inext >= ntaps):
                a+= 1
            else:
                b += 1
            for k in range(kloop):
                #assert (j+k*L_up) < ntaps
                #y_up2[n*L_up+j] += samples[n-k] * taps_up[j+k*L_up]
                y_up2[n*L_up+j] += window[(window_head-k)%window_len] * taps_up[j+k*L_up]  ## window[(window_head-k)%window_len] equals samples[n-k]
    print("a",a)
    print("b",b)

    return y_up1, y_up2  ## , window, window_head






def tst_sparse_conv():
    nsamples = 1024*16
    phase0 = 0.33265221234
    samples = np.exp( 1j * np.arange(nsamples) * np.pi*2/6.1234 + 1j*phase0 ) * np.sin(np.arange(nsamples) * np.pi*2 * 1/1024)
    L_up = 25
    n_taps = L_up*8+0
    taps = firwin(numtaps=n_taps, cutoff=(1/L_up)*0.499, pass_zero=True)
    conv1, conv2 = sparse_conv(samples=samples, L_up=L_up, taps_up=taps)
    diff = len(conv1) - len(conv2)
    print("n taps", n_taps)
    print("len convenctional full convolution: ",len(conv1))
    print("len sparse             convolution: ",len(conv2))
    print("diff: ", diff)
    assert diff >= 0

    i00 = 1024
    compare_len = 1024*3

    errors = list()
    shifts = list()
    for i_shift in range(-(diff+106),(diff+106)):
        errors.append( np.sum(np.abs( conv2[i00:i00+compare_len] - conv1[i_shift+i00:i_shift+i00+compare_len] )) )
        shifts.append(i_shift)
    print("Smallest error:       ", np.min(errors))
    print("average error:        ", np.average(errors))
    print("argmin error:         ",np.argmin(errors))
    print("ishift[argmin error]: ",shifts[np.argmin(errors)])




def tstfirwin():
    from kuokka.lib_tools import radionoise
    from mtools.tools_dsp import fft_usual
    from mtools.tools_math import rollsmooth

    nsamples = 1024* 32 * 2
    samples = radionoise(nsamples, 1,1)
    tapsA = firwin(181, cutoff=0.25, pass_zero=True)
    tapsB = firwin(181, cutoff=0.25, pass_zero=True, fs=1)

    samplesA = np.convolve(samples, tapsA)
    samplesB = np.convolve(samples, tapsB)

    fftA, freqsA = fft_usual(iq_arr=samplesA, srate=1.0, take_abs=True)
    fftB, freqsB = fft_usual(iq_arr=samplesB, srate=1.0, take_abs=True)

    fftA = rollsmooth(fftA, 16)
    fftB = rollsmooth(fftB, 16)

    fig = plt.figure(figsize=(17,12))
    ax1 = fig.add_subplot(111)

    ax1.plot(freqsA, fftA, label="A (fs=default 2)")
    ax1.plot(freqsB, fftB, label="B (fs=1)")

    ax1.grid()
    fig.set_layout_engine("tight")
    plt.show()








if __name__ == '__main__':
    tst1()
    #tst_sparse_conv()
    #tstfirwin()
