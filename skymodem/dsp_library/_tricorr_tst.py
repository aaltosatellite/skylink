import numpy as np
from matplotlib import pyplot as plt


def get_cut_x_axis(x_axis0,  correlated_result):
	x_def = len(x_axis0) - len(correlated_result)
	cut0 = int(x_def/2)
	cut1 = x_def - cut0
	new_x = x_axis0[cut0: -cut1]
	print(cut0, cut1)
	print(len(new_x), len(correlated_result))
	assert len(new_x) == len(correlated_result)
	return new_x


def effect_of_triangle_width():
	sps = 17
	fftlen = 1024
	baudrate = 9600
	df = sps*baudrate / fftlen
	sr = sps*baudrate
	samples = np.random.normal(0, 1, fftlen) + 1j* np.random.normal(0, 1, fftlen)
	fft = np.abs(np.fft.fftshift(np.fft.fft(samples)))
	freqs = np.fft.fftshift( np.fft.fftfreq(fftlen, d=1/sr) )

	n_tx_wid = int(fftlen / sps)
	i_mid = fftlen // 2
	fft[ i_mid:i_mid + n_tx_wid ] += np.average(fft) * 3.5 * (0.5 + np.cos(np.linspace(-np.pi/2, np.pi/2, n_tx_wid))**(1/3))
	x_mid = freqs[i_mid + n_tx_wid//2] + df * (n_tx_wid*0.5 -  n_tx_wid//2)

	triangle0 = 1.0 - np.abs(np.linspace(-1,1, n_tx_wid))
	triangle_p2 = 1.0 - np.abs(np.linspace(-1,1, n_tx_wid+10))
	triangle_m2 = 1.0 - np.abs(np.linspace(-1,1, n_tx_wid-10))

	corr0 = np.correlate(fft, triangle0)
	corr_p2 = np.correlate(fft, triangle_p2)
	corr_m2 = np.correlate(fft, triangle_m2)

	x_fft = freqs
	x_corr0 = get_cut_x_axis(x_axis0=x_fft, correlated_result=corr0)
	x_corr_p2 = get_cut_x_axis(x_axis0=x_fft, correlated_result=corr_p2)
	x_corr_m2 = get_cut_x_axis(x_axis0=x_fft, correlated_result=corr_m2)

	fig = plt.figure(figsize=(14,14))
	ax1 = fig.add_subplot(111)

	ax1.plot(x_fft, fft)
	ax1.plot(x_corr0, corr0)
	ax1.plot(x_corr_p2, corr_p2)
	ax1.plot(x_corr_m2, corr_m2)
	ax1.plot([x_mid, x_mid], [0, np.max(corr_p2)], color="black", linestyle="--")
	ax1.grid()

	fig.set_layout_engine("tight")
	plt.show()







effect_of_triangle_width()
















