import os
from kuokka.lib_tools import make_samples1, make_samples2
import time
import numpy as np
from kuokka.dsp_loop import DSPLoop
from kuokka.lib_receiver import DSPConfig
from queue import Queue
from matplotlib import pyplot as plt



def speedbench_raw_modulation(sr, baudrate, BT):
	nbits 		= 120*8
	T_total 	= nbits / baudrate
	bitstring 	= np.random.randint(0,2, nbits)*2 -1
	sps 		= sr / baudrate
	f_offset	= 0.05
	ntaps 		= int(4*sps) + 1
	_ = make_samples1(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.5, shaper_BT_prod=BT, shaper_n_taps=ntaps)
	_ = make_samples1(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.5, shaper_BT_prod=BT, shaper_n_taps=ntaps)
	t0 = time.perf_counter()
	for _ in range(20):
		_ = make_samples1(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.5, shaper_BT_prod=BT, shaper_n_taps=ntaps)
	dt = (time.perf_counter() - t0) / 20
	mod_baudrate  = nbits / dt
	speed_ratio   = T_total / dt

	print("=== make_samples =======================")
	print("sr: {} Ms/s     baudrate: {} sym/s".format( round(sr*1e-6, 2), round(baudrate, 0) ))
	print("BT:             {}".format( round(BT, 2) ))
	#print("")
	print("dt:             {} ms".format( round(1e3*dt, 2) ))
	print("mod baudrate:   {} k/s".format( round(1e-3*mod_baudrate, 2) ))
	print("speed ratio:    {}".format( round(speed_ratio, 1) ))
	print("="*40)
	print("")


def speedbench_raw_modulation2(sr, baudrate, BT):
	nbits 		= 120*8
	T_total 	= nbits / baudrate
	bitstring 	= np.random.randint(0,2, nbits)*2 -1
	sps 		= sr / baudrate
	f_offset	= 0.05
	_ = make_samples2(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.5, shaper_BT_prod=BT)
	_ = make_samples2(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.5, shaper_BT_prod=BT)
	t0 = time.perf_counter()
	for _ in range(20):
		_ = make_samples2(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.5, shaper_BT_prod=BT)
	dt = (time.perf_counter() - t0) / 20
	mod_baudrate  = nbits / dt
	speed_ratio   = T_total / dt


	print("=== make_samples2 ======================")
	print("sr: {} Ms/s     baudrate: {} sym/s".format( round(sr*1e-6, 2), round(baudrate, 0) ))
	print("BT:             {}".format( round(BT, 2) ))
	#print("")
	print("dt:             {} ms".format( round(1e3*dt, 2) ))
	print("mod baudrate:   {} k/s".format( round(1e-3*mod_baudrate, 2) ))
	print("speed ratio:    {}".format( round(speed_ratio, 1) ))
	print("="*40)
	print("")





def speedbench_packet_modulation(sr, baudrate, BT):
	dsp_config = DSPConfig(rx_sr0=sr,  rx_f_tune=437.1e6, rx_f_center=437.125e6, tx_sr0=sr, tx_f_tune=437.1e6, tx_f_center=437.125e6, baudrate=baudrate, bufferlen=800000, batch_maxlen=16000)
	dsp_config.tx_BT = BT
	dsploop = DSPLoop(dsp_config=dsp_config, que_tx_samples_out=Queue(10), que_tx_payloads_in=Queue(10), que_rx_payloads_out=Queue(10), que_rx_samples_in=Queue(10))

	pl = os.urandom(200)

	samples, _, _ = dsploop._compose_samples(payload=pl, usrp_reshape=False, as_c64=True)
	_, _, _ = dsploop._compose_samples(payload=pl, usrp_reshape=False, as_c64=True)
	_, _, (dt1,dt2,dt3) = dsploop._compose_samples(payload=pl, usrp_reshape=False, as_c64=True)
	t0 = time.perf_counter()
	for _ in range(100):
		dsploop._compose_samples(payload=pl, usrp_reshape=False, as_c64=True)
	T_call = (time.perf_counter() - t0) / 100
	speed_bytes 	= len(pl) / T_call
	speed_bits 		= len(pl)*8 / T_call
	speed_ratio_1 	= speed_bits / baudrate
	speed_ratio_2 	= (len(samples)/T_call) / sr
	print("dt1,dt2,dt3:    ({}, {}, {}) µs".format( *[round(1e6*dt, 1) for dt in (dt1,dt2,dt3)] ))
	print("="*40)
	print("sr: {} Ms/s     baudrate: {} sym/s".format( round(sr*1e-6, 2), round(baudrate, 0) ))
	print("BT:             {}".format( round(BT, 2) ))
	#print("")
	#print("T_call:         {} ms".format( round(1e3*T_call, 2) ))
	#print("mod byterate:   {} kbyte/s".format( round(1e-3*speed_bytes, 2) ))
	print("speed ratio by baudrate:     {}".format( round(speed_ratio_1, 1) ))
	print("speed ratio by samplerate:   {}".format( round(speed_ratio_2, 1) ))
	print("="*40)
	print("")


def plot_modulation_comparison(sr, baudrate, BT):
	nbits 		= 110*8
	bitstring 	= np.random.randint(0,2, nbits)*2 -1
	sps 		= sr / baudrate
	f_offset	= 0.1
	ntaps1 		= int(4*sps) + 1
	samples1, modulator1     = make_samples1(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.5, shaper_BT_prod=BT, shaper_n_taps=ntaps1)
	samples2, modulator2     = make_samples2(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.5, shaper_BT_prod=BT)

	xm1 = np.arange(len(modulator1))
	xm2 = np.arange(len(modulator2)) * len(samples2)/len(modulator2)

	xs1 = np.arange(len(samples1))
	xs2 = np.arange(len(samples2))

	fig = plt.figure(figsize=(12,8))
	ax1 = fig.add_subplot(211)
	ax2 = fig.add_subplot(212)

	ax1.plot(xm1[0:2000], modulator1[0:2000], marker="x")
	ax1.plot(xm2[0:2000], modulator2[0:2000], marker="x")

	ax2.plot(xs1[0:13000], samples1.real[0:13000] - samples2.real[0:13000])
	#ax2.plot(xs2[0:9000], )

	ax1.grid()
	ax2.grid()
	fig.set_layout_engine("tight")
	plt.show()




#speedbench_raw_modulation(sr=1e6, baudrate=1 * 9600, BT=-1)
#speedbench_raw_modulation(sr=1e6, baudrate=2 * 9600, BT=-1)
#speedbench_raw_modulation(sr=1e6, baudrate=4 * 9600, BT=-1)

#speedbench_raw_modulation(sr=3e6, baudrate=9600 //2, BT=0.5)
#speedbench_raw_modulation(sr=1e6, baudrate=9600 * 1, BT=0.5)
#speedbench_raw_modulation(sr=1e6, baudrate=9600 * 2, BT=0.5)
#speedbench_raw_modulation(sr=1e6, baudrate=9600 * 4, BT=0.5)

#speedbench_raw_modulation2(sr=3e6, baudrate=9600 //2, BT=0.5)
#speedbench_raw_modulation2(sr=1e6, baudrate=9600 * 1, BT=0.5)
#speedbench_raw_modulation2(sr=1e6, baudrate=9600 * 2, BT=0.5)
#speedbench_raw_modulation2(sr=1e6, baudrate=9600 * 4, BT=0.5)


#plot_modulation_comparison(sr=1.0e6, baudrate=9600*2, BT=0.5)


print("")
print("#####################################################")
print("#####################################################")
print("")

#speedbench_packet_modulation(sr=1e6, baudrate=1 * 9600, BT=-1)
#speedbench_packet_modulation(sr=1e6, baudrate=2 * 9600, BT=-1)
#speedbench_packet_modulation(sr=1e6, baudrate=4 * 9600, BT=-1)
#print("#####################")
#speedbench_packet_modulation(sr=1e6, baudrate=1 * 9600, BT=0.5)
#speedbench_packet_modulation(sr=1e6, baudrate=2 * 9600, BT=0.5)
#speedbench_packet_modulation(sr=1e6, baudrate=4 * 9600, BT=0.5)
#print("#####################")
speedbench_packet_modulation(sr=3.6e6, baudrate=9600 //2, BT=0.5)
speedbench_packet_modulation(sr=3.6e6, baudrate=1 * 9600, BT=0.5)
speedbench_packet_modulation(sr=3.6e6, baudrate=2 * 9600, BT=0.5)
speedbench_packet_modulation(sr=3.6e6, baudrate=4 * 9600, BT=0.5)


















