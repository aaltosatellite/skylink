import os
from sdr_recorder import get_samples, fpaths
from kuokka.lib_tools import make_samples
import time
import numpy as np
from kuokka.radio_loop import RadioLoop
from kuokka.lib_receiver import ReceiverConfig
from mtools.tools_dsp import waterfall_mx
from matplotlib import pyplot as plt
from scipy.signal import firwin





def test_sample_modulation_speed(sr, baudrate, BT):
	#T_total 	= 0.050
	#nbits 		= int(T_total * baudrate)

	nbits 		= 200*8
	T_total 	= nbits / baudrate

	bitstring 	= np.random.randint(0,2, nbits)*2 -1
	sps 		= sr / baudrate
	f_offset	= 12.0e3 / sr

	samples 	= make_samples(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.7,  shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=201)
	samples 	= make_samples(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.7,  shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=201)

	t0 = time.perf_counter()
	samples 	= make_samples(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.7,  shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=201)
	dt = time.perf_counter() - t0
	mod_baudrate  = nbits / dt
	speed_ratio   = T_total / dt

	print("="*40)
	print("sr:             {} Ms/s".format( round(sr*1e-6, 2) ))
	print("baudrate:       {} /s".format( round(baudrate, 0) ))
	print("BT:             {}".format( round(BT, 2) ))

	print("")
	print("dt:             {} ms".format( round(1e3*dt, 2) ))
	print("mod baudrate:   {} k/s".format( round(1e-3*mod_baudrate, 2) ))
	print("speed ratio:    {}".format( round(speed_ratio, 1) ))
	print("="*40)
	print("")





def test_packet_generation_speed(sr, baudrate, BT):
	rx_config = ReceiverConfig(sr0=sr, baudrate=baudrate, bufferlen=800000, batch_maxlen=16000, f_tune=437e6, f_center=437.025e6)
	rx_config.BT = BT
	radioloop = RadioLoop(rx_config=rx_config)
	pl = os.urandom(200)

	samples, _ = radioloop._compose_samples(payload=pl, usrp_reshape=False, as_c64=True)
	samples, _ = radioloop._compose_samples(payload=pl, usrp_reshape=False, as_c64=True)

	t0 = time.perf_counter()
	for _ in range(100):
		radioloop._compose_samples(payload=pl, usrp_reshape=False, as_c64=True)
	T_call = (time.perf_counter() - t0) / 100
	speed_bytes 	= len(pl) / T_call
	speed_bits 		= len(pl)*8 / T_call
	speed_ratio_1 	= speed_bits / baudrate
	speed_ratio_2 	= (len(samples)/T_call) / sr
	print("="*40)
	print("sr:             {} Ms/s".format( round(sr*1e-6, 2) ))
	print("baudrate:       {} /s".format( round(baudrate, 0) ))
	print("BT:             {}".format( round(BT, 2) ))
	print("")
	print("T_call:         {} ms".format( round(1e3*T_call, 2) ))
	print("mod byterate:   {} kbyte/s".format( round(1e-3*speed_bytes, 2) ))
	print("speed ratio by baudrate:     {}".format( round(speed_ratio_1, 1) ))
	print("speed ratio by samplerate:   {}".format( round(speed_ratio_2, 1) ))
	print("="*40)
	print("")





def compare_generated_to_recording(fpath):
	recorded = get_samples(fpath)
	recorded = recorded / np.average( np.abs(recorded))

	rx_config = ReceiverConfig(sr0=1e6, baudrate=9600, bufferlen=800000, batch_maxlen=16000, f_tune=437.00e6, f_center=437.1e6)
	rx_config.sps = 41
	rx_config.BT = -1
	rx_config.mod_index = 0.5
	radioloop = RadioLoop(rx_config=rx_config)
	n_init_silence = int(1e6 * (0.0414 + 0.1))
	samples = np.zeros( n_init_silence, dtype=np.complex128 )
	pl = os.urandom(36)
	for _ in range(7):
		gen_samples = radioloop._compose_samples(payload=pl, usrp_reshape=False, as_c64=True)
		samples = np.concatenate( (samples, gen_samples) )
		#samples = np.concatenate( (samples, np.zeros(n_gap, dtype=np.complex128)) )
	nn = len(samples)

	recorded[0:nn] += samples

	waterfall_mx(recorded, fftlen=2048, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=True)






def plot_fmdemod_of_recording(fpath):
	recorded = get_samples(fpath)
	recorded = recorded / np.average( np.abs(recorded))
	N0 = len(recorded)
	recorded = recorded[0 : int(N0* 0.12)]
	recorded = recorded * np.exp(2j * np.pi * np.arange(len(recorded)) * (1/1e6) * -0.1244e6)
	waterfall_mx(recorded, fftlen=2048, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=True)

	lpfilter = firwin(numtaps=201, cutoff=0.63 * 3 * 9600 / 1e6, pass_zero=True)
	recorded = np.convolve(recorded, lpfilter)
	waterfall_mx(recorded, fftlen=2048, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=True)

	zz = recorded * np.conj( np.roll(recorded, 1) )
	dmd = np.arctan2(zz.imag, zz.real)
	xt = np.arange(len(dmd)) * 1 / 1e6

	t_gap_hyp = 0.12224 - 0.11675
	print("Silence hypothesis: {} ms".format( t_gap_hyp * 1e3 ))
	cursor_array = [
		0.04218,
		0.11675,
	]

	fig = plt.figure(figsize=(14,14))
	ax = fig.add_subplot(111)
	ax.plot(xt - 0.0, dmd)
	for c in cursor_array:
		ax.plot( [c,c], [0,1], color="black", linestyle="--" )
		ax.plot( [c+t_gap_hyp,c+t_gap_hyp], [0,1], color="red", linestyle="--" )
	ax.grid()
	fig.set_layout_engine("tight")
	plt.show()












test_sample_modulation_speed(sr=1e6, baudrate=1*9600,  BT=-1)
test_sample_modulation_speed(sr=1e6, baudrate=2*9600,  BT=-1)
test_sample_modulation_speed(sr=1e6, baudrate=4*9600,  BT=-1)
test_sample_modulation_speed(sr=1e6, baudrate=8*9600,  BT=-1)
test_sample_modulation_speed(sr=1e6, baudrate=9600,    BT=0.5)
test_sample_modulation_speed(sr=1e6, baudrate=9600*2,  BT=0.5)
test_sample_modulation_speed(sr=1e6, baudrate=9600*4,  BT=0.5)
print("################################################")
print("")
print("")
test_packet_generation_speed(sr=1e6, baudrate=1*9600, BT=-1)
test_packet_generation_speed(sr=1e6, baudrate=2*9600, BT=-1)
test_packet_generation_speed(sr=1e6, baudrate=1*9600, BT=0.5)
test_packet_generation_speed(sr=1e6, baudrate=2*9600, BT=0.5)

"""
test_packet_generation_speed(sr=1e6, baudrate=1*9600, BT=-1)
test_packet_generation_speed(sr=1e6, baudrate=2*9600, BT=-1)
test_packet_generation_speed(sr=1e6, baudrate=4*9600, BT=-1)
test_packet_generation_speed(sr=1e6, baudrate=8*9600, BT=-1)
test_packet_generation_speed(sr=1e6, baudrate=1*9600, BT=0.5)
test_packet_generation_speed(sr=1e6, baudrate=2*9600, BT=0.5)
print("################################################")
print("")
print("")
"""

#compare_generated_to_recording()
#plot_fmdemod_of_recording()



















