import numpy as np
from kuokka.lib_symsynching import create_classic_JPL_statemx
from kuokka.lib_demodulation import create_DSD_statemx
import time
from mtools.tools_dsp import waterfall_mx
from kuokka.lib_tools import radionoise
import os, pickle
from matplotlib import pyplot as plt
from kuokka.lib_receiver import RXDSPConfig, Receiver



fpath1 = "/home/elmore/datasetit/radiotallenteet/uhf-radioloop-capture-GY.pkl"
fpath2 = "/home/elmore/datasetit/radiotallenteet/uhf-radioloop-capture-WJ.pkl"
fpath3 = "/home/elmore/datasetit/radiotallenteet/uhf-radioloop-capture-RQ.pkl"
for fp in (fpath1,fpath2,fpath3):
	assert os.path.isfile(fp)




def draw(draw_time_series=False):
	f = open(fpath2, "rb")
	rd = f.read()
	f.close()
	samples = pickle.loads(rd)
	print("Loaded {}".format( str(type(samples)) ))
	print("Of len {}".format( len(samples) ))
	nn = len(samples)
	t_array = np.arange(nn) * (1/1e6)

	if draw_time_series:
		fig = plt.figure(figsize=(12,8))
		fig.set_layout_engine("tight")
		ax = fig.add_subplot(111)
		ax.plot(t_array[::30], np.abs(samples)[::30])
		ax.grid()
		plt.show()

	i0 = int(1e6 * 2.83)
	i1 = int(1e6 * 2.91)
	fshifter = np.exp(2j*np.pi * np.arange(0,i1-i0) * (0.12))
	samples[i0:i1] = samples[i0:i1] * fshifter

	config = RXDSPConfig(rx_sr0=1e6, rx_f_tune=437.066e6, rx_f_center=437.125e6, baudrate=9600, bufferlen=int(len(samples) * 0.3), batch_maxlen=1024 * 16)
	config.centerf_halflife = 12
	r_rate = config.get_r_rate()
	rx = Receiver(config=config)
	batchlen = int(0.5*1024/r_rate)
	fftcorr_mx = list()

	c = 0
	while c < len(samples):
		rx.process_batch(batch=samples[c:c+batchlen], give_bits=False)
		c += batchlen
		line = rx.FFTstatemx[2,:]*1.0
		line[np.argmax(line)] *= 100
		fftcorr_mx.append( line )

	fftcorr_mx = np.array(fftcorr_mx)
	fftcorr_mx = np.log(fftcorr_mx+1)

	fig = plt.figure(figsize=(12,8))
	fig.set_layout_engine("tight")
	ax = fig.add_subplot(111)
	ax.imshow(fftcorr_mx)
	ax.grid()
	plt.show()



draw(draw_time_series=True)

