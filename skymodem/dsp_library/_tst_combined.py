import numpy as np
from kuokka.lib_symsynching import create_classic_JPL_statemx
from kuokka.lib_demodulation import create_DSD_statemx, demod_synch_decide
import time
from kuokka.lib_tools import radionoise



def speedbench_demod_synch_decide(sps, baudrate, lp_ntaps):
	batchlen = 1024
	n_rep = 40
	nsamples = int(batchlen * (n_rep+2) + 10)
	sr = baudrate * sps
	# ====================================
	lp_cutoff 		= 9600 * 0.63 / sr
	JPL_n_decay 	= 12
	JPL_delay_mpr	= 6.0
	# ====================================
	samples = radionoise(n=nsamples, sr=sr, W_per_Hz=1.0)
	center_f_arr = np.zeros(nsamples, dtype=np.float64) + 0.1
	dmd_arr = np.zeros(nsamples, dtype=np.float64)
	synch_arr = np.zeros((nsamples,3), dtype=np.int64)
	JPLstatemx = create_classic_JPL_statemx(N_eps=sps, n_decay=JPL_n_decay)
	demodmx = create_DSD_statemx(lp_ntaps=lp_ntaps, lp_cutoff=lp_cutoff, synch_delay_mpr_f=JPL_delay_mpr, sps_f=float(sps))

	demod_synch_decide(rs_arr=samples, center_f_arr=center_f_arr, i_rs0=0, nsamples=batchlen, dmd_arr=dmd_arr, synch_arr=synch_arr, dmdsynch_head0=0, JPLstatemx=JPLstatemx.copy(), demodmx=demodmx.copy())
	demod_synch_decide(rs_arr=samples, center_f_arr=center_f_arr, i_rs0=0, nsamples=batchlen, dmd_arr=dmd_arr, synch_arr=synch_arr, dmdsynch_head0=0, JPLstatemx=JPLstatemx.copy(), demodmx=demodmx.copy())
	demod_synch_decide(rs_arr=samples, center_f_arr=center_f_arr, i_rs0=0, nsamples=batchlen, dmd_arr=dmd_arr, synch_arr=synch_arr, dmdsynch_head0=0, JPLstatemx=JPLstatemx.copy(), demodmx=demodmx.copy())
	rs_head = 0
	dmd_head = 0
	t0 = time.perf_counter()
	for _ in range(n_rep):
		dmd_head, _, _ = demod_synch_decide(rs_arr=samples, center_f_arr=center_f_arr, i_rs0=rs_head, nsamples=batchlen, dmd_arr=dmd_arr, synch_arr=synch_arr, dmdsynch_head0=dmd_head, JPLstatemx=JPLstatemx, demodmx=demodmx)
		rs_head += batchlen
	T_total = (time.perf_counter() - t0)
	T_sample = T_total/(batchlen * n_rep)
	speed = 1/T_sample
	overmatch = speed / sr
	core_fraction	= (1/overmatch) / 1.0
	budget_fraction	= (1/overmatch) / 0.5

	print("")
	print("-- demod_synch_decide() ---------------------")
	print("T-sample:        {} ns/sample".format( round(1e9*T_sample, 1) ))
	print("speed:           {} Ms/s".format( round(1e-6*speed, 3) ))
	print("overmatch:       {}".format( round( overmatch, 3) ))
	print("core use:        {} %".format( round(100 * core_fraction, 1) ))
	print("budget use:      {} %".format( round(100 * budget_fraction, 1) ))
	print("----------------------------------------------------")
	print("")








if __name__ == '__main__':
	speedbench_demod_synch_decide(sps=20, baudrate=9600, lp_ntaps=161)
	speedbench_demod_synch_decide(sps=14, baudrate=9600*4, lp_ntaps=161)
	speedbench_demod_synch_decide(sps=17, baudrate=9600, lp_ntaps=121)
	speedbench_demod_synch_decide(sps=8, baudrate=9600*16, lp_ntaps=121)















