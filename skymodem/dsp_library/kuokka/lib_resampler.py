import numpy as np
from numba import njit
import scipy

RESAMP_STATE_BOUNDARY = 1.0
RESAMP_STATE_INTERP   = 2.0

def _kaiser_beta_as(stopband_att):
	as_ = abs(stopband_att)
	if as_ > 50:
		beta = 0.1102*(as_ - 8.7)
	elif as_ > 21.0:
		beta = 0.5842 * (as_-21)**0.4 + 0.07886*(as_ - 21)
	else:
		beta = 0.0
	return beta

def _kaiser(i,n,beta):
	t = i - (n-1)/2
	r = 2*t/(n-1)
	a = scipy.special.iv(0, beta * np.sqrt(1-r*r))
	b = scipy.special.iv(0, beta)
	return a / b

def firdes_kaiser(n, f_cutoff, stopband_att, frac_samp_offset):
	assert -0.5 < frac_samp_offset < 0.5
	assert 0 < f_cutoff < 0.5
	assert n > 0
	assert stopband_att > 0
	beta = _kaiser_beta_as(stopband_att)
	h = np.zeros(n, dtype=np.float64)
	for i in range(n):
		t = i - (n-1)/2 + frac_samp_offset
		h1 = np.sinc(2 * f_cutoff * t)
		h2 = _kaiser(i,n,beta)
		h[i] = h1 * h2
	return h

def create_resampler(m_halflen, n_banks, r_rate, f_cutoff, allow_aliasing=False): # TODO name the f_cutoff to c_cutoff? It is renormalized
	assert m_halflen > 1
	assert type(m_halflen) == int
	assert n_banks > 1
	assert r_rate > 0
	#assert 0 < f_cutoff < r_rate*0.5
	if not allow_aliasing:
		if not (f_cutoff < (r_rate*0.5)):
			print("[cutoff should be less than {}]".format(r_rate*0.5))
			raise ValueError("rate & cutoff would lead to aliasing. You can allow aliasing with 'allow_aliasing' argument.")
	ntaps = 2 * m_halflen * n_banks + 1
	lp_taps = firdes_kaiser(n=ntaps, f_cutoff=f_cutoff/n_banks, stopband_att=60, frac_samp_offset=0.0)  #f_cutoff=f_cutoff/n_banks

	lp_taps = np.array(lp_taps)
	gain = 0.0
	for i in range(ntaps):
		gain += lp_taps[i]
	gain = n_banks / gain
	lp_taps = lp_taps * gain

	bank = np.zeros( (n_banks, 2*m_halflen) , dtype=np.complex128)
	for i in range(n_banks):
		for j in range(2*m_halflen):
			bank[i,j] = lp_taps[i + j*n_banks]
	dtau = 1.0 / r_rate
	tau = 0.0
	bf = 0.0
	b = 0
	mu = 0.0
	state = RESAMP_STATE_BOUNDARY
	y0_y1 = np.zeros(2, dtype=np.complex128)
	window = np.zeros(2*m_halflen, dtype=np.complex128)
	statemx = np.zeros( (n_banks+1+1+1, 2*m_halflen), dtype=np.complex128 )
	statemx[0:n_banks,:] = bank								# bank (static)
	statemx[n_banks] = window								# window (
	statemx[n_banks+1,0:6] = tau,dtau,bf,b,mu,state
	statemx[n_banks+2,0:2] = y0_y1
	return statemx

@njit(cache=True)
def set_rate(statemx, r_rate):
	statemx[-2][1] = 1.0 / r_rate

@njit(cache=True)
def adjust_rate(statemx, gamma):
	statemx[-2][1] = statemx[-2][1] / gamma


@njit(cache=True)
def _timing_update(tau, dtau, n_banks):  #tau_dtau_bf_b_mu_state
	tau = tau + dtau
	bf = tau * n_banks
	b = int(np.floor(bf))
	mu = bf - b
	return tau,dtau,bf,b,mu

@njit(cache=True)
def resampler_execute(samples, statemx):   #old resampler code in old_sources.txt
	assert np.iscomplexobj(samples)
	assert np.iscomplexobj(statemx)
	nsp = len(samples)
	n_banks = len(statemx) - 3
	tau, dtau, bf, b, mu, state = (statemx[n_banks + 1][0:6]).real
	b = int(b)
	y0,y1 = statemx[n_banks + 2][0:2]
	window = statemx[n_banks]
	y = np.zeros(int(nsp / dtau) + 2, dtype=np.complex128)
	iy = 0
	for s in samples:
		window = np.roll(window, 1)
		window[0] = s
		while b < n_banks:
			if state == RESAMP_STATE_BOUNDARY:
				y1 = np.dot(window, statemx[0])
				y[iy] = (1 - mu)*y0 + mu*y1
				iy += 1
				tau,dtau,bf,b,mu = _timing_update(tau,dtau,n_banks)
				state = RESAMP_STATE_INTERP
			else:
				y0 = np.dot(window, statemx[b])
				if b == (n_banks - 1):
					state = RESAMP_STATE_BOUNDARY
					b = n_banks
				else:
					y1 = np.dot(window, statemx[b + 1])
					y[iy] = (1 - mu)*y0 + mu*y1
					iy += 1
					tau,dtau,bf,b,mu = _timing_update(tau,dtau,n_banks)
		tau = tau - 1.0
		bf = bf - n_banks
		b = b - n_banks
	statemx[n_banks] = window
	statemx[n_banks + 1][0:6] = tau,dtau,bf,float(b),mu,state
	statemx[n_banks + 2][0:2] = y0,y1
	return y[:iy]



@njit(cache=True)
def resampler_execute_stream(in_arr, ii0, nsamples, out_arr, io0, statemx): # not sensitive to the given indexing (ii0, io0). That is to say, rolling these arrays in between calls is ok.
	assert np.iscomplexobj(in_arr)
	assert np.iscomplexobj(out_arr)
	assert np.iscomplexobj(statemx)
	n_banks = len(statemx) - 3
	tau, dtau, bf, b, mu, state = (statemx[n_banks + 1][0:6]).real
	b = int(b)
	y0,y1 = statemx[n_banks + 2][0:2]
	window = statemx[n_banks]
	rs_head = io0
	for ii in range(ii0, ii0+nsamples):
		window = np.roll(window, 1)
		window[0] = in_arr[ii]
		while b < n_banks:
			if state == RESAMP_STATE_BOUNDARY:
				y1 = np.dot(window, statemx[0])
				out_arr[rs_head] = (1 - mu)*y0 + mu*y1
				rs_head += 1
				tau,dtau,bf,b,mu = _timing_update(tau,dtau,n_banks)
				state = RESAMP_STATE_INTERP
			else:
				y0 = np.dot(window, statemx[b])
				if b == (n_banks - 1):
					state = RESAMP_STATE_BOUNDARY
					b = n_banks
				else:
					y1 = np.dot(window, statemx[b + 1])
					out_arr[rs_head] = (1 - mu)*y0 + mu*y1
					rs_head += 1
					tau,dtau,bf,b,mu = _timing_update(tau,dtau,n_banks)
		tau = tau - 1.0
		bf = bf - n_banks
		b = b - n_banks
	statemx[n_banks] = window
	statemx[n_banks + 1][0:6] = tau,dtau,bf,float(b),mu,state
	statemx[n_banks + 2][0:2] = y0,y1
	return rs_head

