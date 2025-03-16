import os
from kuokka.lib_tools import make_samples
import time
import numpy as np
from kuokka.radio_loop import RadioLoop
from kuokka.lib_receiver import ReceiverConfig





def speedbench_raw_modulation(sr, baudrate, BT):
	nbits 		= 200*8
	T_total 	= nbits / baudrate
	bitstring 	= np.random.randint(0,2, nbits)*2 -1
	sps 		= sr / baudrate
	f_offset	= 12.0e3 / sr

	_ = make_samples(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.7,  shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=201)
	_ = make_samples(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.7,  shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=201)
	t0 = time.perf_counter()
	_ = make_samples(sps_f=sps,  bitstring=bitstring, f_offset=f_offset, power=1.0, mod_index=0.7,  shaper_mode=1, shaper_BT_prod=BT, shaper_n_taps=201)
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





def speedbench_packet_modulation(sr, baudrate, BT):
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








speedbench_raw_modulation(sr=1e6, baudrate=1 * 9600, BT=-1)
speedbench_raw_modulation(sr=1e6, baudrate=2 * 9600, BT=-1)
speedbench_raw_modulation(sr=1e6, baudrate=4 * 9600, BT=-1)
speedbench_raw_modulation(sr=1e6, baudrate=9600, BT=0.5)
speedbench_raw_modulation(sr=1e6, baudrate=9600 * 2, BT=0.5)
speedbench_raw_modulation(sr=1e6, baudrate=9600 * 4, BT=0.5)
print("")
print("#####################################################")
print("#####################################################")
print("")
speedbench_packet_modulation(sr=1e6, baudrate=1 * 9600, BT=-1)
speedbench_packet_modulation(sr=1e6, baudrate=2 * 9600, BT=-1)
speedbench_packet_modulation(sr=1e6, baudrate=4 * 9600, BT=-1)
speedbench_packet_modulation(sr=1e6, baudrate=1 * 9600, BT=0.5)
speedbench_packet_modulation(sr=1e6, baudrate=2 * 9600, BT=0.5)
speedbench_packet_modulation(sr=1e6, baudrate=4 * 9600, BT=0.5)




















