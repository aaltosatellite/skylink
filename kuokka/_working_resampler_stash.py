import numpy as np
from scipy.signal import firwin
from matplotlib import pyplot as plt
from scipy.signal import windows
import scipy
STATE_BOUNDARY = 1.0
STATE_INTERP   = 2.0


def kaiser_beta_as(stopband_att):
	as_ = abs(stopband_att)
	if as_ > 50:
		beta = 0.1102*(as_ - 8.7)
	elif as_ > 21.0:
		beta = 0.5842 * (as_-21)**0.4 + 0.07886*(as_ - 21)
	else:
		beta = 0.0
	return beta

def kaiser(i,n,beta):
	t = i - (n-1)/2
	r = 2*t/(n-1)
	#a = besseli0f(beta * np.sqrt(1-r*r) )
	#b = besseli0f(beta)
	a = scipy.special.iv(0, beta * np.sqrt(1-r*r))
	b = scipy.special.iv(0, beta)
	return a / b


def firdes_kaiser(n, f_cutoff, stopband_att, frac_samp_offset):
	assert -0.5 < frac_samp_offset < 0.5
	assert 0 < f_cutoff < 0.5
	assert n > 0
	assert stopband_att > 0
	beta = kaiser_beta_as(stopband_att)
	h = np.zeros(n, dtype=np.float64)
	for i in range(n):
		t = i - (n-1)/2 + frac_samp_offset
		h1 = np.sinc(2 * f_cutoff * t)
		h2 = kaiser(i,n,beta)
		h[i] = h1 * h2
	return h






def create_resampler(m_halflen, n_banks, r_rate, f_cutoff, sr):
	ntaps = 2 * m_halflen * n_banks + 1
	lp_taps = firwin(numtaps=ntaps, cutoff=f_cutoff, fs=sr, pass_zero=True)
	lp_taps = windows.kaiser(M=ntaps, beta=600, sym=True)
	lp_taps = firdes_kaiser(n=ntaps, f_cutoff=f_cutoff/n_banks, stopband_att=60, frac_samp_offset=0.0)
	#lp_taps = windows.taylor(ntaps, nbar=20, sll=60, norm=False) * (0.6/m_halflen)

	lp_taps = np.array(lp_taps)
	gain = 0.0
	for i in range(ntaps):
		gain += lp_taps[i]
	gain = n_banks / gain
	#lp_taps = lp_taps * gain

	bank = np.zeros( (n_banks, 2*m_halflen) )
	for i in range(n_banks):
		for j in range(2*m_halflen):
			bank[i,j] = lp_taps[i + j*n_banks]
	dtau = 1 / r_rate # = "del"
	tau = 0.0
	bf = 0.0
	b = 0
	mu = 0.0
	state = STATE_BOUNDARY
	# n_banks = "num_filters" = "npfb"
	# 2*m_halflen = "h_sub_len"
	tau_dtau_bf_b_mu_state = np.array( (tau, dtau, bf, b, mu, state) )
	y0_y1 = np.zeros(2, dtype=np.complex128)
	window = np.zeros(2*m_halflen, dtype=np.complex128)
	return bank, tau_dtau_bf_b_mu_state, y0_y1, window



def resampler_execute(samples, bank, tau_dtau_bf_b_mu_state, y0_y1, window):
	nsp = len(samples)
	y = np.zeros(int(nsp / tau_dtau_bf_b_mu_state[1]) + 2, dtype=np.complex128)
	iy = 0
	n_banks = len(bank)
	for s in samples:
		window = np.roll(window, 1)
		window[0] = s
		while tau_dtau_bf_b_mu_state[3] < n_banks:
			if tau_dtau_bf_b_mu_state[-1] == STATE_BOUNDARY:
				y0_y1[1] = np.dot(window, bank[0])
				y[iy] = (1 - tau_dtau_bf_b_mu_state[4])*y0_y1[0] + tau_dtau_bf_b_mu_state[4]*y0_y1[1]
				iy += 1
				tau_dtau_bf_b_mu_state = timing_update(tau_dtau_bf_b_mu_state, n_banks)
				tau_dtau_bf_b_mu_state[-1] = STATE_INTERP
			else:
				y0_y1[0] = np.dot(window, bank[int(tau_dtau_bf_b_mu_state[3])])
				if np.isclose(tau_dtau_bf_b_mu_state[3], (n_banks - 1)):
					tau_dtau_bf_b_mu_state[-1] = STATE_BOUNDARY
					tau_dtau_bf_b_mu_state[3] = n_banks
				else:
					y0_y1[1] = np.dot(window, bank[int(tau_dtau_bf_b_mu_state[3]+1)])
					y[iy] = (1 - tau_dtau_bf_b_mu_state[4])*y0_y1[0] + tau_dtau_bf_b_mu_state[4]*y0_y1[1]
					iy += 1
					tau_dtau_bf_b_mu_state = timing_update(tau_dtau_bf_b_mu_state, n_banks)
		tau_dtau_bf_b_mu_state[0] = tau_dtau_bf_b_mu_state[0] - 1.0
		tau_dtau_bf_b_mu_state[2] = tau_dtau_bf_b_mu_state[2] - n_banks
		tau_dtau_bf_b_mu_state[3] = tau_dtau_bf_b_mu_state[3] - n_banks
	print("rel: {} vs {}".format(iy, len(y)))
	return y[:iy], tau_dtau_bf_b_mu_state, y0_y1, window


def timing_update(tau_dtau_bf_b_mu_state, n_banks):
	tau_dtau_bf_b_mu_state[0] = tau_dtau_bf_b_mu_state[0] + tau_dtau_bf_b_mu_state[1]
	tau_dtau_bf_b_mu_state[2] = tau_dtau_bf_b_mu_state[0] * n_banks
	tau_dtau_bf_b_mu_state[3] =  np.floor(tau_dtau_bf_b_mu_state[2])
	tau_dtau_bf_b_mu_state[4] = tau_dtau_bf_b_mu_state[2] - tau_dtau_bf_b_mu_state[3]
	return tau_dtau_bf_b_mu_state





def tst_resampling_1():
	t_arr = np.linspace(0, 2*np.pi*8, 600)
	x_arr = np.sin(t_arr)
	x_arr = x_arr + np.cos(t_arr*2.5+1)*0.6
	x_arr = x_arr * np.cos(np.linspace(-np.pi/2, np.pi/2, 600))**3

	t20 = t_arr[::3]
	x20 = x_arr[::3]
	dt20 = t20[1] - t20[0]
	mm = 13
	r_rate = np.random.random()*2
	#r_rate = 1.660675038467
	print("rate factor: {}".format(r_rate))
	bank, tau_dtau_bf_b_mu_state, y0_y1, window = create_resampler(m_halflen=mm, n_banks=64, r_rate=r_rate, f_cutoff=0.499, sr=1)

	y, tau_dtau_bf_b_mu_state, y0_y1, window = resampler_execute(samples=x20, bank=bank, tau_dtau_bf_b_mu_state=tau_dtau_bf_b_mu_state, y0_y1=y0_y1, window=window)
	ty = np.arange(len(y)) * dt20 / r_rate
	ty = ty - dt20 * mm

	fig = plt.figure(figsize=(14,12))
	ax = fig.add_subplot(111)
	ax.plot(t_arr, x_arr.real)
	ax.scatter(t20, x20)
	ax.plot(ty, y.real, marker="+")
	ax.plot(ty, y.imag)
	ax.grid()
	fig.set_layout_engine("tight")
	plt.show()






tst_resampling_1()




















