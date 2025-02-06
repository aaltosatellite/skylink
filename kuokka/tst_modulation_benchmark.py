from kuokka.lib_tools import make_samples
import time
import numpy as np





def test_sample_modulation_speed(sr, baudrate, BT):
	T_total 	= 0.050

	nbits 		= int(T_total * baudrate)
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




test_sample_modulation_speed(sr=1e6, baudrate=1*9600,  BT=-1)
test_sample_modulation_speed(sr=1e6, baudrate=2*9600,  BT=-1)
test_sample_modulation_speed(sr=1e6, baudrate=4*9600,  BT=-1)
test_sample_modulation_speed(sr=1e6, baudrate=8*9600,  BT=-1)
test_sample_modulation_speed(sr=1e6, baudrate=16*9600, BT=-1)
test_sample_modulation_speed(sr=1e6, baudrate=9600,    BT=0.5)


























