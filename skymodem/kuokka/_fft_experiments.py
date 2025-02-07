import cmath
import numpy as np
import numba as nb


@nb.jit
def ilog2(n):
	result = -1
	if n < 0:
		n = -n
	while n > 0:
		n >>= 1
		result += 1
	return result


@nb.njit(fastmath=True)
def reverse_bits(val, width):
	result = 0
	for _ in range(width):
		result = (result << 1) | (val & 1)
		val >>= 1
	return result


@nb.njit(fastmath=True)
def fft_1d_radix2_rbi(arr, direct=True):
	arr = np.asarray(arr, dtype=np.complex128)
	n = len(arr)
	levels = ilog2(n)
	e_arr = np.empty_like(arr)
	coeff = (-2j if direct else 2j) * cmath.pi / n
	for i in range(n):
		e_arr[i] = cmath.exp(coeff * i)
	result = np.empty_like(arr)
	for i in range(n):
		result[i] = arr[reverse_bits(i, levels)]
	# Radix-2 decimation-in-time FFT
	size = 2
	while size <= n:
		half_size = size // 2
		step = n // size
		for i in range(0, n, size):
			k = 0
			for j in range(i, i + half_size):
				temp = result[j + half_size] * e_arr[k]
				result[j + half_size] = result[j] - temp
				result[j] += temp
				k += step
		size *= 2
	return result


@nb.njit(fastmath=True)
def fft_1d_arb(arr, fft_1d_r2=fft_1d_radix2_rbi):
	"""1D FFT for arbitrary inputs using chirp z-transform"""
	arr = np.asarray(arr, dtype=np.complex128)
	n = len(arr)
	m = 1 << (ilog2(n) + 2)
	e_arr = np.empty(n, dtype=np.complex128)
	for i in range(n):
		e_arr[i] = cmath.exp(-1j * cmath.pi * (i * i) / n)
	result = np.zeros(m, dtype=np.complex128)
	result[:n] = arr * e_arr
	coeff = np.zeros_like(result)
	coeff[:n] = e_arr.conjugate()
	coeff[-n + 1:] = e_arr[:0:-1].conjugate()
	return fft_convolve(result, coeff, fft_1d_r2)[:n] * e_arr / m


@nb.njit(fastmath=True)
def fft_convolve(a_arr, b_arr, fft_1d_r2=fft_1d_radix2_rbi):
	return fft_1d_r2(fft_1d_r2(a_arr) * fft_1d_r2(b_arr), False)


@nb.njit(fastmath=True)
def fft_1d(arr):
	n = len(arr)
	if not n & (n - 1):
		return fft_1d_radix2_rbi(arr)
	else:
		return fft_1d_arb(arr)



@nb.njit(cache=True)
def compute_fft(x):
	#y = np.zeros_like(x, dtype=np.complex128)
	with nb.objmode(y='complex128[:]'):
		y = np.complex128(np.fft.fft(x))
		#y = np.fft.fft(x)
	return y


"""
@nb.njit()
def compute_fft(xx):
	x = np.random.randint(100)
	fft_x = compute_fft_(x)
	return fft_x
"""

if __name__ == '__main__':
	import time
	nn = 1024 + 13
	a = np.random.normal(0, 1, nn) + np.random.normal(0, 1, nn) * 1j

	same_as = np.allclose( np.fft.fft(a), fft_1d(a) )
	print("(fft_1d same as np.fft.fft:       {})".format(same_as))
	print("")

	k = compute_fft(a)
	k_same_as = np.allclose( np.fft.fft(a), compute_fft(a) )
	print("(compute_fft same as np.fft.fft:  {})".format( k_same_as ))
	print("")



	np.fft.fft(a)
	np.fft.fft(a)
	t0 = time.perf_counter()
	for _ in range(100):
		x = np.fft.fft(a)
	dt_ref = (time.perf_counter() - t0) / 100
	speed_ref = 1/dt_ref
	print("np.fft.fft")
	print("="*40)
	print("T call fft_1d:    {} µs".format(round(1e6 * dt_ref, 2)))
	print("speed:            {} k/s".format(round(1e-3 * speed_ref, 2)))
	print("="*40)
	print("")


	fft_1d(a)
	fft_1d(a)
	fft_1d(a)
	t0 = time.perf_counter()
	for _ in range(100):
		x = fft_1d(a)
	dt_1 = (time.perf_counter() - t0) / 100
	speed_1 = 1/dt_1
	print("fft_1d")
	print("="*40)
	print("T call fft_1d:    {} µs".format(round(1e6 * dt_1, 2)))
	print("speed:            {} k/s".format(round(1e-3 * speed_1, 2)))
	print("="*40)
	print("")


	compute_fft(a)
	compute_fft(a)
	t0 = time.perf_counter()
	for _ in range(100):
		x = compute_fft(a)
	dt_2 = (time.perf_counter() - t0) / 100
	speed_2 = 1/dt_2
	print("compute_fft")
	print("="*40)
	print("T call fft_1d:    {} µs".format(round(1e6 * dt_2, 2)))
	print("speed:            {} k/s".format(round(1e-3 * speed_2, 2)))
	print("="*40)
	print("")








	print("speed ratio: fft_1d / ref        {}".format( round(speed_1 / speed_ref, 2) ))
	print("speed ratio: compute_fft / ref   {}".format( round(speed_2 / speed_ref, 2) ))































