import numpy as np
from numba import njit
import scipy
from .lib_resampler import create_resampler, resampler_execute_stream

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




def create_div_resampler(m_halflen, div, f_cutoff, allow_aliasing=False): # TODO name the f_cutoff to c_cutoff? It is renormalized
	assert m_halflen > 3
	assert type(m_halflen) == int
	assert type(div) in (int, np.int64)
	assert div >= 2
	r_rate = 1 / div
	if not allow_aliasing:
		if not (f_cutoff < (r_rate*0.5)):
			print("[cutoff should be less than {}]".format(r_rate*0.5))
			raise ValueError("rate & cutoff would lead to aliasing. You can allow aliasing with 'allow_aliasing' argument.")
	ntaps = 2 * m_halflen + 1
	lp_taps = firdes_kaiser(n=ntaps, f_cutoff=f_cutoff, stopband_att=60, frac_samp_offset=0.0)  #f_cutoff=f_cutoff/n_banks
	lp_taps = np.array(lp_taps)
	gain = 0.0
	for i in range(ntaps):
		gain += lp_taps[i]
	gain = 1.0 / gain  #1.0 was n_banks
	lp_taps = lp_taps * gain
	statemx = np.zeros( (ntaps+3, ntaps), dtype=np.complex128 )
	for i in range(ntaps):
		statemx[2+i,:] = np.roll(lp_taps, -ntaps+1+i)
	statemx[0,0] = div
	statemx[0,1] = 0
	statemx[0,2] = 0
	return statemx




@njit(cache=True)
def div_resampler_execute(in_arr, statemx): # not sensitive to the given indexing (ii0, io0). That is to say, rolling these arrays in between calls is ok.
	assert np.iscomplexobj(in_arr)
	assert np.iscomplexobj(statemx)
	div = int(statemx[0,0].real)
	win_idx = int(statemx[0,1].real)
	div_idx = int(statemx[0,2].real)
	winlen = statemx.shape[1]
	window = statemx[1,:]
	n_out = int(2+len(in_arr)/div)
	out_arr = np.zeros(n_out, dtype=np.complex128)
	io = 0
	for ii in range(len(in_arr)):
		window[win_idx] = in_arr[ii]
		if div_idx == 0:
			out_arr[io] = np.dot(window, statemx[win_idx+2,:])
			io += 1
		div_idx = (div_idx+1) % div
		win_idx = (win_idx+1) % winlen
	statemx[0,1] = win_idx
	statemx[0,2] = div_idx
	return out_arr[0:io]




@njit(cache=True)
def div_resampler_execute_stream(in_arr, ii0, nsamples, out_arr, io0, statemx): # not sensitive to the given indexing (ii0, io0). That is to say, rolling these arrays in between calls is ok.
	assert np.iscomplexobj(in_arr)
	assert np.iscomplexobj(out_arr)
	assert np.iscomplexobj(statemx)
	div = int(statemx[0,0].real)
	win_idx = int(statemx[0,1].real)
	div_idx = int(statemx[0,2].real)
	winlen = statemx.shape[1]
	window = statemx[1,:]
	io = io0
	for ii in range(ii0, ii0+nsamples):
		window[win_idx] = in_arr[ii]
		if div_idx == 0:
			out_arr[io] = np.dot(window, statemx[win_idx+2,:])
			io += 1
		div_idx = (div_idx+1) % div
		win_idx = (win_idx+1) % winlen
	statemx[0,1] = win_idx
	statemx[0,2] = div_idx
	return io





def create_staged_resampler(halflen_div, halflen_f, r_rate, n_banks, f_cutoff, allow_aliasing=False): # TODO name the f_cutoff to c_cutoff? It is renormalized
	assert type(halflen_div) in (int, np.int64)
	assert type(halflen_f) in (int, np.int64)
	assert type(n_banks) in (int, np.int64)
	for x in (halflen_div, halflen_f, n_banks):
		assert x > 2
	assert r_rate < 1.0
	if not allow_aliasing:
		if not (f_cutoff < (r_rate*0.5)):
			print("[cutoff should be less than {}]".format(r_rate*0.5))
			raise ValueError("rate & cutoff would lead to aliasing. You can allow aliasing with 'allow_aliasing' argument.")
	div = int(1/r_rate)
	r_rate_frac = r_rate*div
	mx1 = create_div_resampler(m_halflen=halflen_div, div=div, f_cutoff=f_cutoff, allow_aliasing=allow_aliasing)
	mx2 = create_resampler(m_halflen=halflen_f, n_banks=n_banks, r_rate=r_rate_frac, f_cutoff=0.499*r_rate_frac, allow_aliasing=allow_aliasing)
	if np.isclose(r_rate_frac, 1.0):
		mx1[0,4] = 1
	return mx1, mx2


@njit(cache=True)
def staged_resampler_execute_stream(in_arr, ii0, nsamples, out_arr, io0, mx1, mx2):
	assert np.iscomplexobj(in_arr)
	assert np.iscomplexobj(out_arr)
	assert np.iscomplexobj(mx1)
	assert np.iscomplexobj(mx2)
	only_div_stage = int(mx1[0,4].real)
	if only_div_stage:
		return div_resampler_execute_stream(in_arr=in_arr, ii0=ii0, nsamples=nsamples, out_arr=out_arr, io0=io0, statemx=mx1)
	io1 = div_resampler_execute_stream(in_arr=in_arr, ii0=ii0, nsamples=nsamples, out_arr=out_arr, io0=io0, statemx=mx1)
	io2 = resampler_execute_stream(in_arr=out_arr, ii0=io0, nsamples=io1-io0, out_arr=out_arr, io0=io0, statemx=mx2)
	return io2


















