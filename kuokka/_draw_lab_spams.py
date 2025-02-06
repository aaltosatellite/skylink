import numpy as np
from mtools.tools_dsp import waterfall_mx
from matplotlib import pyplot as plt
from scipy.signal import firwin
import pickle

fpath1 = "./Lab_USRP_spam5.txt"
fpath2 = "./Lab_USRP_spam7.txt"
fpath3 = "./Lab_USRP_spam8.txt"

fpath4 = "/home/elmore/datasetit/radiotallenteet/uhf-nayte-76.dat"
fpath5 = "/home/elmore/datasetit/radiotallenteet/uhf-nayte-96.dat"

fpaths = [fpath1,fpath2,fpath3]

fpaths2 = [fpath4,fpath5]



def get_samples2(idx):
	f = open(fpaths2[idx], "rb")
	rd = f.read()
	f.close()
	samples = pickle.loads(rd)
	assert type(samples) == np.ndarray
	return samples


def get_samples(idx):
	f = open(fpaths[idx], "r")
	rd = f.read()
	f.close()
	samples = list()
	lines = rd.split("\n")
	for line in lines:
		if not line:
			continue
		if not line[0] == "(":
			if line.endswith("j"):
				s = float(line[:-1]) * 1j
			else:
				s = float(line)
			samples.append(s)
			print(line)
			print(s)
			print("")
			continue
		assert line[0] == "(", line
		assert line[-1] == ")", line

		line = line[1:-2]
		if line.endswith("+0"):
			s = float(line[:-2])
			samples.append(s)
			continue

		assert line.count(".") == 2
		i = 7 + line[7:].index(".") - 2
		rel = line[0:i]
		img = line[i:]
		s = float(rel) + float(img)*1j
		samples.append( s )
		#print(line)
		#print(s)
	samples = np.array(samples, dtype=np.complex128)
	return samples

def draw(idx):
	samples = get_samples(idx)
	#samples = samples[250000:-500000]
	sr0 = 0.5e6 / 1.66666666
	samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) *  (1/sr0) * -86.5e3*sr0/1e6)

	waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, srate=sr0, plot_and_show=True, y_is_time=True)

	lpfilter = firwin(numtaps=201, cutoff=0.6*(4/1.6666)*9600/sr0, pass_zero=True)

	samples = np.convolve(samples, lpfilter)

	zz = samples * np.conj( np.roll(samples, 1) )
	dmd = np.arctan2(zz.imag, zz.real)

	xx0 = np.arange(len(dmd))
	xx_t = np.arange(len(dmd)) * (1/sr0)
	xx_sym = np.arange(len(dmd)) * (1/sr0) / (1/9600)

	fig = plt.figure(figsize=(13,13))
	ax1 = fig.add_subplot(111)

	ax1.plot(xx_sym, dmd )

	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()




if __name__ == '__main__':
	draw(1)






























