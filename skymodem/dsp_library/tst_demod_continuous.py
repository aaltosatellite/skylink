import numpy as np
from kuokka.lib_demodulation import demodulation_sequence, create_DSD_statemx
from kuokka.lib_symsynching import create_classic_JPL_statemx






def tst_demodulation_continuity():
	sps 			= 17
	JPLdecay 		= 16
	synch_delay_mpr = 8
	lp_cutoff_coeff = 0.63
	lp_ntaps 		= 171
	baudrate 		= 9600
	sr 				= sps*baudrate

	noiseamp = 0.7
	samples 		= np.zeros(0, dtype=np.complex128)
	center_f_array 	= np.zeros(0, dtype=np.float64)
	n_signal_segments = np.random.randint(0, 90)
	signal_total = 0
	print("Testing demodulation is continuous / batch boundary agnostic")
	print("{} signal segments".format(n_signal_segments))
	for i in range(n_signal_segments):
		if np.random.random() > 0.15:
			noiselen = np.random.randint(0, sps*900)
			noise = np.random.normal(0, noiseamp, noiselen) + 1j*np.random.normal(0, noiseamp, noiselen)
			samples = np.concatenate( (samples, noise) )
			center_f_array = np.concatenate( (center_f_array, np.zeros(noiselen)-1.0 ) )

		if np.random.random() > 0.15:
			siglen = np.random.randint(0, sps*600)
			signal_total += siglen
			f_signal = (np.random.random()-0.5)*2 * 0.4
			signal = np.random.normal(0, noiseamp, siglen) + 1j*np.random.normal(0, noiseamp, siglen)
			samples = np.concatenate( (samples, signal) )
			center_f_array = np.concatenate( (center_f_array, np.zeros(siglen)+f_signal ) )

		if np.random.random() > 0.15:
			noiselen = np.random.randint(0, sps*10)
			noise = np.random.normal(0, noiseamp, noiselen) + 1j*np.random.normal(0, noiseamp, noiselen)
			samples = np.concatenate( (samples, noise) )
			center_f_array = np.concatenate( (center_f_array, np.zeros(noiselen)-1.0 ) )

	nsamples = len(samples)
	print("{} samples".format(nsamples))
	assert len(center_f_array) == len(samples)
	assert signal_total == np.sum( center_f_array > -0.5 )

	dmd_arr1 		= np.zeros(nsamples, dtype=np.float64)
	dmd_arr2 		= np.zeros(nsamples, dtype=np.float64)
	synch_arr1 		= np.zeros((nsamples,3), dtype=np.int64)
	synch_arr2 		= np.zeros((nsamples,3), dtype=np.int64)
	bitarr1 		= np.zeros(3*int(nsamples/sps))
	bitarr2 		= np.zeros(3*int(nsamples/sps))
	bitfarr1 		= np.zeros(3*int(nsamples/sps), dtype=np.float64)
	bitfarr2 		= np.zeros(3*int(nsamples/sps), dtype=np.float64)

	JPLstatemx1 = create_classic_JPL_statemx(N_eps=sps, n_halflife=JPLdecay)
	JPLstatemx2 = create_classic_JPL_statemx(N_eps=sps, n_halflife=JPLdecay)
	DSD_statemx1 = create_DSD_statemx(lp_ntaps=lp_ntaps, lp_cutoff_coeff=lp_cutoff_coeff, synch_delay_mpr_f=synch_delay_mpr, sps_f=sps)
	DSD_statemx2 = create_DSD_statemx(lp_ntaps=lp_ntaps, lp_cutoff_coeff=lp_cutoff_coeff, synch_delay_mpr_f=synch_delay_mpr, sps_f=sps)

	dmd_head1, bit_head1 = demodulation_sequence(rs_arr=samples, centerf_arr=center_f_array, i_rs0=0, nsamples=nsamples, dmd_arr=dmd_arr1,
						  synch_arr=synch_arr1, dmdsynch_head0=0, JPLstatemx=JPLstatemx1, demodmx=DSD_statemx1, bitarr=bitarr1, bitfarr=bitfarr1, bit_head0=0)
	assert dmd_head1 == signal_total, (dmd_head1, signal_total)

	head2 = 0
	bit_head2 = 0
	dmd_head2 = 0
	nchunks = 0
	while head2 < nsamples:
		n_process = np.random.randint(0,  2000)
		n_process = min(n_process, nsamples - head2)
		dmd_head2, bit_head2 = demodulation_sequence(rs_arr=samples, centerf_arr=center_f_array, i_rs0=head2, nsamples=n_process, dmd_arr=dmd_arr2,
						  synch_arr=synch_arr2, dmdsynch_head0=dmd_head2, JPLstatemx=JPLstatemx2, demodmx=DSD_statemx2, bitarr=bitarr2, bitfarr=bitfarr2, bit_head0=bit_head2)
		head2 += n_process
		nchunks += 1
	assert dmd_head2 == signal_total
	assert dmd_head1 == dmd_head2

	assert bit_head1 == bit_head2
	assert np.all( abs(bitarr1[0:bit_head1]) > 0.5 )
	assert np.allclose(bitarr1[0:bit_head1], bitarr2[0:bit_head2])
	assert np.allclose(dmd_arr1[0:dmd_head1], dmd_arr2[0:dmd_head2])
	assert np.allclose(synch_arr1[0:dmd_head1], synch_arr2[0:dmd_head2])

	print("ALL CLEAR. 1 vs {} chunks".format(nchunks))
	print("")










if __name__ == '__main__':
	for _ in range(2000):
		tst_demodulation_continuity()









