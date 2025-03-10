import time
import numpy as np
#from matplotlib import pyplot as plt
from kuokka.lib_symsynching import create_classic_JPL_statemx, classic_JPL_synch_run, classic_JPL_synch_step, classic_JPL_synch_strm
from kuokka.lib_symsynching import general_JPL_synch_run_2, create_general_JPL_statemx_2, general_JPL_synch_step_2
from kuokka.lib_symsynching import general_JPL_synch_run_1, general_JPL_synch_step_1, create_general_JPL_statemx_1



def _synchs_against_eachother_round(plott=False):
	sps = np.random.randint(11,100) + (np.random.random()-0.5)*0.005
	n_samples = int(sps * 1024)
	stream = np.sin( np.arange(n_samples)*np.pi/sps )
	stream = np.sign(stream) * np.abs(stream)**0.33
	stream = stream + np.random.normal(0, 0.33, n_samples)
	approximate_zeros = np.arange(1024)*sps #*(np.random.random()+0.3)
	if plott:
		from matplotlib import pyplot as plt
		fig = plt.figure(figsize=(14,9))
		ax = fig.add_subplot(111)
		ax.plot(np.arange(n_samples), stream)
		ax.grid()
		fig.set_layout_engine("tight")
		plt.show()
	N_eps = int(round(sps))

	mx_clss   = create_classic_JPL_statemx(N_eps=N_eps, n_decay=12)
	mx_gene_1 = create_general_JPL_statemx_1(sps_int=N_eps, n_decay=12, c_constant=1.0, c_shape=0.0, shape_idx=0)
	mx_gene_2 = create_general_JPL_statemx_2(sps_int=N_eps, n_decay=12, c_constant=1.0, c_shape=0.0, shape_idx=0)

	out_clss = classic_JPL_synch_run(samples=stream, statemx=mx_clss)
	out_gene_1 = general_JPL_synch_run_1(samples=stream, statemx=mx_gene_1)
	out_gene_2 = general_JPL_synch_run_2(samples=stream, statemx=mx_gene_2)
	assert np.allclose(out_clss, out_gene_1)
	assert np.allclose(out_clss, out_gene_2)
	# synch runs produce equal outputs

	out_clss_strm = np.zeros( (len(stream), 3), dtype=np.int64)
	mx_clss   = create_classic_JPL_statemx(N_eps=N_eps, n_decay=12)
	synch_head = classic_JPL_synch_strm(sample_arr=stream, i_sample0=0, nsamples=len(stream), synch_arr=out_clss_strm, synch_head0=0, statemx=mx_clss)
	assert np.allclose(out_clss_strm, out_clss)
	assert synch_head == len(stream)
	# stream synch (classic) produces the same as single shot run

	mx_clss   = create_classic_JPL_statemx(N_eps=N_eps, n_decay=12)
	mx_gene_1 = create_general_JPL_statemx_1(sps_int=N_eps, n_decay=12, c_constant=1.0, c_shape=0.0, shape_idx=0)
	mx_gene_2 = create_general_JPL_statemx_2(sps_int=N_eps, n_decay=12, c_constant=1.0, c_shape=0.0, shape_idx=0)
	for i,s in enumerate(stream):
		ring_amax_clss, ring_idx_clss = classic_JPL_synch_step(sample=s, statemx=mx_clss)
		ring_amax_gene_1, ring_idx_gene_1 = general_JPL_synch_step_1(sample=s, statemx=mx_gene_1)
		ring_amax_gene_2, ring_idx_gene_2 = general_JPL_synch_step_2(sample=s, statemx=mx_gene_2)
		assert ring_amax_clss == ring_amax_gene_1
		assert ring_amax_clss == ring_amax_gene_2
		assert ring_idx_clss  == ring_idx_gene_1
		assert ring_idx_clss  == ring_idx_gene_2
		assert out_clss[i][0] == ring_amax_clss
		assert out_clss[i][1] == ring_idx_clss
		# stepping synchronizers produces equal outputs wrt each other, and to the single shot runs above

	dmin_arr = list()
	for isym in range(30, 400):
		approx_zero_i = int(round(approximate_zeros[isym]))
		d1 = (out_clss[approx_zero_i][0] - out_clss[approx_zero_i][1]) % N_eps
		d2 = (out_clss[approx_zero_i][1] - out_clss[approx_zero_i][0]) % N_eps
		dmin = min(d1,d2)
		dmin_arr.append(dmin)
	avg_min_distance = np.average(dmin_arr)
	assert avg_min_distance < 1.1
	# the synchronizer aligns on average ~1 sample from the correct synch. (obs: sps can be up to 100)




def tst_synchs_against_eachother():
	print("-----------------------------------------------------------")
	print("Testing JPL synchronizer variants produce equal outputs")
	NN = 30
	for ii in range(NN):
		_synchs_against_eachother_round(plott=False)
		if (ii%5) == 0:
			print("{}/{}".format(ii,NN))
	print("{}/{}".format(NN,NN))
	print("-----------------------------------------------------------")





def timing_classic():
	statemx = create_classic_JPL_statemx(N_eps=17, n_decay=12)

	samples = np.random.normal(0,1, 3000000)

	classic_JPL_synch_step(sample=samples[0], statemx=statemx)
	classic_JPL_synch_step(sample=samples[1], statemx=statemx)
	classic_JPL_synch_step(sample=samples[2], statemx=statemx)

	t0 = time.perf_counter()
	for ii, s in enumerate(samples):
		classic_JPL_synch_step(sample=s, statemx=statemx)
	dt_call = (time.perf_counter() - t0) / len(samples)
	speed = 1/dt_call
	print("T-call: {} µs".format( round(1e6*dt_call, 4) ))
	print("speed:  {} Ms/s".format( round(1e-6 * speed, 4) ))
	print("")
	return speed











if __name__ == '__main__':
	tst_synchs_against_eachother()
	S = 0.0
	for _ in range(10):
		S += timing_classic()
	print("Average:")
	print(S/10)














