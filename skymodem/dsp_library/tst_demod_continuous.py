import numpy as np
from kuokka.lib_demodulation import create_DD_statemx, create_demod_statemx, demodulate
from kuokka.lib_symsynching import create_classic_JPL_statemx
from matplotlib import pyplot as plt
from kuokka.lib_tools import radionoise, make_samples




def tst_demodulation_continuity():
    sps 			= 10.0
    #JPLdecay 		= 16
    #synch_delay_mpr = 8
    lp_cutoff_coeff = 0.499*sps
    lp_ntaps 		= 171
    baudrate 		= 9600
    sr 				= sps*baudrate
    f_offset		= 0.1
    mod_idx			= 0.5
    n_silence_start	= int(0.1 * sr)
    n_silence_end	= int(0.05 * sr)

    bitstring = np.random.randint(0, 2, 256*8)*2 -1

    samples, _ = make_samples(samples_per_symbol=sps, bitstring=bitstring, frequency_offset=f_offset, power=1.0, modulation_index=mod_idx, shaper_BT_prod=0.5, n_silence_start=n_silence_start, n_silence_end=n_silence_end)
    samples = samples + radionoise(len(samples), sr=sr, W_per_Hz=0.2/baudrate)
    nsamples = len(samples)
    demodmx0 = create_demod_statemx(lp_ntaps=lp_ntaps, lp_cutoff_coeff=lp_cutoff_coeff, sps_f=sps)

    center_f_arr0 = np.zeros(nsamples) + f_offset
    center_f_arr1 = np.zeros(nsamples) + f_offset + 0.1 * 8/sps
    center_f_arr2 = np.zeros(nsamples) + f_offset + 0.2 * 8/sps
    center_f_arr3 = np.zeros(nsamples) + f_offset + 0.3 * 8/sps
    center_f_arr4 = np.zeros(nsamples) + f_offset - 0.4 * 8/sps
    dmd_arr0 = np.zeros(nsamples)
    dmd_arr1 = np.zeros(nsamples)
    dmd_arr2 = np.zeros(nsamples)
    dmd_arr3 = np.zeros(nsamples)
    dmd_arr4 = np.zeros(nsamples)

    print("Demodulating.")
    demodulate(sample_arr=samples, center_f_arr=center_f_arr0, sample_i0=0, demod_n_samples=nsamples, dmd_arr=dmd_arr0, demodmx=demodmx0.copy())
    demodulate(sample_arr=samples, center_f_arr=center_f_arr1, sample_i0=0, demod_n_samples=nsamples, dmd_arr=dmd_arr1, demodmx=demodmx0.copy())
    demodulate(sample_arr=samples, center_f_arr=center_f_arr2, sample_i0=0, demod_n_samples=nsamples, dmd_arr=dmd_arr2, demodmx=demodmx0.copy())
    demodulate(sample_arr=samples, center_f_arr=center_f_arr3, sample_i0=0, demod_n_samples=nsamples, dmd_arr=dmd_arr3, demodmx=demodmx0.copy())
    demodulate(sample_arr=samples, center_f_arr=center_f_arr4, sample_i0=0, demod_n_samples=nsamples, dmd_arr=dmd_arr4, demodmx=demodmx0.copy())
    print("")

    stdlen   = int(sps*8*8)
    std_arr0 = np.array([np.std(dmd_arr0[i:i+stdlen]) for i in range(0, nsamples-stdlen)])
    std_arr1 = np.array([np.std(dmd_arr1[i:i+stdlen]) for i in range(0, nsamples-stdlen)])
    std_arr2 = np.array([np.std(dmd_arr2[i:i+stdlen]) for i in range(0, nsamples-stdlen)])
    std_arr3 = np.array([np.std(dmd_arr3[i:i+stdlen]) for i in range(0, nsamples-stdlen)])
    std_arr4 = np.array([np.std(dmd_arr4[i:i+stdlen]) for i in range(0, nsamples-stdlen)])

    fig = plt.figure(figsize=(14,12))
    fig.set_layout_engine("tight")
    ax1 = fig.add_subplot(121)
    ax2 = fig.add_subplot(122)
    ax1.plot(np.arange(nsamples), dmd_arr0)
    ax1.plot(np.arange(nsamples), dmd_arr1)
    ax1.plot(np.arange(nsamples), dmd_arr2)
    ax1.plot(np.arange(nsamples), dmd_arr3)
    ax1.plot(np.arange(nsamples), dmd_arr4)
    ax1.grid()

    ax2.plot(np.arange(nsamples-stdlen), std_arr0)
    ax2.plot(np.arange(nsamples-stdlen), std_arr1)
    ax2.plot(np.arange(nsamples-stdlen), std_arr2)
    ax2.plot(np.arange(nsamples-stdlen), std_arr3)
    ax2.plot(np.arange(nsamples-stdlen), std_arr4)
    ax2.grid()
    plt.show()













if __name__ == '__main__':
    tst_demodulation_continuity()
