import numpy as np
from numba import njit
import scipy
from scipy.signal import firwin

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





## === fractional resampler ==================================================================================================================================================================
## ===========================================================================================================================================================================================
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
	#fshift_phase = 1j * statemx[n_banks + 1][6].real			# this line, and the three below, would be an inbuilt frequency shifter, that needs an extra argument "fshift_nrm"
	b = int(b)
	y0,y1 = statemx[n_banks + 2][0:2]
	window = statemx[n_banks]
	rs_head = io0
	for ii in range(ii0, ii0+nsamples):
		window = np.roll(window, 1)
		window[0] = in_arr[ii] #* np.exp(fshift_phase)
		#fshift_phase += 2j*np.pi*fshift_nrm
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
	#statemx[n_banks + 1][6] = fshift_phase.imag
	statemx[n_banks + 2][0:2] = y0,y1
	return rs_head
## === fractional resampler ==================================================================================================================================================================
## ===========================================================================================================================================================================================







## === discrete resampler ====================================================================================================================================================================
## ===========================================================================================================================================================================================
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
## === discrete resampler ====================================================================================================================================================================
## ===========================================================================================================================================================================================







## === staged resampler ======================================================================================================================================================================
## ===========================================================================================================================================================================================
def minimal_disc_halflen_for_staged_resampler(r_rate, f_cutoff, min_f_undisturbed, minimum_value=8, require_total_sampling=True):
	"""
	Computes the lowest value of halflen_div for 'create_div_resampler()' 'create_staged_resampler()' such that the maximum undisturbed frequency of the
	fractional resampler stage is higher than 'min_f_undisturbed'.
	"""
	assert 0.0 < f_cutoff < (0.5*r_rate)
	assert 0.0 < r_rate <= 0.5
	assert min_f_undisturbed < f_cutoff
	assert min_f_undisturbed > 0.0
	assert type(minimum_value) == int
	assert minimum_value > 0
	f_mid_d = f_cutoff
	halflen_div = minimum_value
	minimal_total_sampling_length = 2 + (int(1/r_rate) - 1) / 2
	while True:
		halfwidth_d = 0.94 / halflen_div
		limit = f_mid_d - halfwidth_d  # f_cutoff * sr1  -  0.94 * sr0 / m_div                 ==   f_cutoff_coeff * r_rate  - 0.94 / m_disc
		a = limit >= min_f_undisturbed
		b = (not require_total_sampling) or (halflen_div >= minimal_total_sampling_length)
		if a and b:
			return halflen_div
		halflen_div += 1


def minimal_frac_halflen_for_staged_resampler(halflen_div, r_rate, f_cutoff, minimum_value=8):
	"""
	Computes the lowest value of 'halflen_f' for 'create_staged_resampler()' such that the maximum undisturbed frequency of the
	fractional resampler stage is higher than that of the discrete resampler stage.
	"""
	assert 0.0 < f_cutoff < (0.5*r_rate)
	assert 0.0 < r_rate <= 0.5
	assert type(halflen_div) == int
	assert halflen_div > 0
	assert type(minimum_value) == int
	assert minimum_value > 0
	f_mid_d = f_cutoff
	halfwidth_d = 0.94 / halflen_div
	lim_d = f_mid_d - halfwidth_d  # f_cutoff * sr1  -  0.94 * sr0 / m_div                 ==   f_cutoff_coeff * r_rate  - 0.94 / m_disc
	f_mid_frac = 0.499 * r_rate
	halflen_frac = minimum_value
	while True:
		halfwidth_frac = 0.94 * (1/int(1/r_rate)) / halflen_frac
		lim_frac = f_mid_frac - halfwidth_frac	# 0.499 * sr1  -  0.94 * (sr0/int(sr0/sr1)) / m_f    ==   0.499 * r_rate  -  0.94 * 1/int(1/r_rate) / m_frac
		if lim_frac > (lim_d - 0.01):
			return halflen_frac
		halflen_frac += 1


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
	mid_arr = np.zeros(nsamples+10, dtype=in_arr.dtype)
	io1 = div_resampler_execute_stream(in_arr=in_arr, ii0=ii0, nsamples=nsamples, out_arr=mid_arr, io0=0, statemx=mx1)
	io2 = resampler_execute_stream(in_arr=mid_arr, ii0=0, nsamples=io1, out_arr=out_arr, io0=io0, statemx=mx2)
	return io2
## === staged resampler ======================================================================================================================================================================
## ===========================================================================================================================================================================================







## === upsampler =============================================================================================================================================================================
## ===========================================================================================================================================================================================
@njit(cache=True)
def cubic_interp(y, rate, x_minus1, y_minus1, y_minus2):
	#assert rate > 1
	len2_apprx = int((len(y)+1) * rate + 6)
	intp = np.zeros(len2_apprx, dtype=y.dtype)
	dx = 1/rate
	i_out = 0
	m_arr = np.zeros(len(y), dtype=y.dtype)
	m_arr[1:len(y)-1] = (m_arr[2:] - m_arr[0:-2]) * 0.5  # m_i = (y[i+2] - y[i-1]) / 2  #todo: tabulate these first. If i repeats, we repeat the same calculation.
	m_arr[0] = (y[1] - y_minus1) * 0.5
	m_minus1 = (y[0] - y_minus2) * 0.5
	#m_arr[-1] = 0.0   # OR just stop one shy...
	x = x_minus1
	lstop = len(y)-1
	while x < 1:
		t = x % 1
		a = (2*t**3 - 3*t**2 + 1) * y_minus1
		b = (t**3 - 2*t**2 + t) * m_minus1
		c = (-2*t**3 + 3*t**2) * y[0]
		d = (t**3 - t**2) * m_arr[0]
		intp[i_out] = a+b+c+d
		x += dx
		i_out += 1
	x = x - 1
	while x < lstop:  # can be parallelized. (for i_out in range... xi = x0 + dx*i_out
		t = x % 1
		i = int(x)
		a = (2*t**3 - 3*t**2 + 1) * y[i]
		b = (t**3 - 2*t**2 + t) * m_arr[i]
		c = (-2*t**3 + 3*t**2) * y[i+1]
		d = (t**3 - t**2) * m_arr[i+1]
		intp[i_out] = a+b+c+d
		x += dx
		i_out += 1
	return intp[:i_out], x-lstop, y[-1], y[-2]


@njit(cache=True)
def linear_interp(y, rate, x_minus1, y_minus1):
	#assert rate > 1
	len2_apprx = int((len(y)+1) * rate + 6)
	intp = np.zeros(len2_apprx, dtype=y.dtype)
	dx = 1/rate
	i_out = 0
	x = x_minus1
	lstop = len(y)-1
	while x < 1:
		t = x % 1
		tp = 1 - t
		a = tp * y_minus1
		b = t * y[0]
		intp[i_out] = a+b
		x += dx
		i_out += 1
	x = x - 1
	while x < lstop:  # can be parallelized. (for i_out in range... xi = x0 + dx*i_out
		t = x % 1
		tp = 1 - t
		i = int(x)
		a = tp * y[i]
		b = t * y[i+1]
		intp[i_out] = a+b
		x += dx
		i_out += 1
	return intp[:i_out], x-lstop, y[-1]


def create_upsample_taps(r_rate, dtype, m_halflen=9, L_limit=16, up_cutoff=0.499):
	"""
	m_halflen: 		Filter length. Larger values increase amplitude precision at the cost of computation time.
					Values smaller than 8 will affect amplitude reconstruction severely.
	L_limit:		Proper integer upsampling is done up to L_limit, and linearly interpolated beyond that.
	"""
	#assert r_rate > 1.0, "This is an upsampler. May break with downsampling, and is surely inefficient. Use linear_interp() instead"
	#if r_rate < 4.0:
	#	raise AssertionError("Ordinary resampler (create_resampler()) is faster and probably more optimal for rates less than ~16, certainly less than 4.")
	assert dtype in (np.float64, np.float32, np.complex128, np.complex64)
	L_up 		= max(min( int(np.ceil(r_rate)), L_limit), 6) ## smaller limit -> faster
	r_fine 		= r_rate / L_up
	n_taps 		= L_up*2*m_halflen+1
	taps_up 	= firwin(numtaps=n_taps, cutoff=(1/L_up)*up_cutoff, fs=1, pass_zero=True) * (L_up+0)
	window_ym1 		= np.zeros(int((n_taps-1-0)/L_up)+1  + 1, dtype=dtype)
	window_head = 0
	delay_estimate 		= ((n_taps+1)/L_up)/2 -1 		# todo: This is not exactly correct, but usually within 99%
	scalar_arr = np.array([L_up, r_fine, window_head, 0.0], dtype=np.float64)
	return scalar_arr, taps_up, window_ym1, delay_estimate


@njit(cache=True)
def upsample(samples, scalar_arr, taps_up, window_ym1):
	"""
	window = np.zeros(int((ntaps-1-0)/L_up)+1, dtype=samples.dtype)
	"""
	L_up = int(scalar_arr[0])
	r_fine = scalar_arr[1]
	window_head = int(scalar_arr[2])
	xm1 = scalar_arr[3]
	ym1 = window_ym1[-1]
	y_up = np.zeros(len(samples)*L_up, dtype=samples.dtype)
	window_len = len(window_ym1)-1
	#for i in range(len(samples)):
	#	y_up[i*L_up] = samples[i]
	#y_up = np.convolve(y_up, taps_up, mode="same")
	#a = 0
	#b = 0
	ntaps = len(taps_up)
	for n in range(len(samples)):
		window_head = (window_head+1)%window_len
		window_ym1[window_head] = samples[n]
		for j in range(L_up):
			kloop = int((ntaps-1-j)/L_up)+1
			#kmax = kloop-1
			#imax = j+kmax*L_up
			#inext = j+(kmax+1)*L_up
			#if (imax < ntaps) and (inext >= ntaps):
			#	a+= 1
			#else:
			#	b += 1
			for k in range(kloop):
				#y_up[n*L_up+j] += samples[n-k] * taps_up[j+k*L_up]
				y_up[n*L_up+j] += window_ym1[(window_head-k)%window_len] * taps_up[j+k*L_up]  ## window[(window_head-k)%window_len] equals samples[n-k]
	#print("a",a)
	#print("b",b)
	scalar_arr[2] = window_head
	if r_fine <= 0:
		return y_up
	#return y_up, window, window_head, 0.0, 0.0, 0.0
	interp, x_minus1, y_minus1 = linear_interp(y_up, rate=r_fine, x_minus1=xm1, y_minus1=ym1)
	#interp, x_minus1, y_minus1, y_minus2 = cubic_interp(y_up, rate=r_fine, x_minus1=0.0, y_minus1=0.0, y_minus2=0.0)
	scalar_arr[3] = x_minus1
	window_ym1[-1] = y_minus1
	return interp
	#return interp, window, window_head, x_minus1, y_minus1, y_minus2
## === upsampler =============================================================================================================================================================================
## ===========================================================================================================================================================================================








