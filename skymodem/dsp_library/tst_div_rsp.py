import time
import numpy as np
from kuokka.lib_div_resampler import create_div_resampler, div_resampler_execute_stream, create_staged_resampler, staged_resampler_execute_stream
from kuokka.lib_resampler import create_resampler, resampler_execute_stream
from kuokka.lib_tools import radionoise, make_samples
from mtools.tools_dsp import waterfall_mx, fft_usual
from mtools.tools_math import rollsmooth
from matplotlib import pyplot as plt



def test_1():
	samples = radionoise(n=int(1e6), sr=1e6, W_per_Hz=0.000001)

	bits = np.random.randint(0,2,8*32)*2 -1
	signal = make_samples(sps_f=1e6/9600, bitstring=bits, f_offset=0.05, power=1.0, mod_index=0.5, shaper_mode=1, shaper_BT_prod=0.5, shaper_n_taps=301, n_silence_start=0, n_silence_end=0)

	samples[100000:100000+len(signal)] += signal

	waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, srate=1e6, plot_and_show=True, y_is_time=True)

	r_rate =  1 / 9.5
	div = int(1/r_rate)
	r_rate_balance = r_rate * div
	halflen = 20

	rspmx = create_resampler(m_halflen=halflen, n_banks=64, r_rate=r_rate_balance, f_cutoff=0.3*r_rate_balance, allow_aliasing=False)
	divrsp_mx = create_div_resampler(m_halflen=halflen, div=div, f_cutoff=0.3/div, allow_aliasing=False)
	mx1, mx2 = create_staged_resampler(halflen_div=halflen, halflen_f=halflen, r_rate=r_rate, n_banks=64, f_cutoff=0.3*r_rate, allow_aliasing=False)

	out1 = np.zeros(int(1e6), dtype=np.complex128)
	out2 = np.zeros(int(1e6), dtype=np.complex128)
	out3 = np.zeros(int(1e6), dtype=np.complex128)
	outx = np.zeros(int(1e6), dtype=np.complex128)

	resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out1, io0=0, statemx=rspmx)
	resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=outx, io0=0, statemx=rspmx)
	div_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out2, io0=0, statemx=divrsp_mx)
	div_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=outx, io0=0, statemx=divrsp_mx)
	io3 = staged_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out3, io0=0, mx1=mx1, mx2=mx2)
	staged_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=outx, io0=0, mx1=mx1, mx2=mx2)

	t0 = time.perf_counter()
	for _ in range(20):
		resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=outx, io0=0, statemx=rspmx)
	dt_call1 = (time.perf_counter() - t0) / 20
	speed1 = len(samples) / dt_call1

	t0 = time.perf_counter()
	for _ in range(20):
		div_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=outx, io0=0, statemx=divrsp_mx)
	dt_call2 = (time.perf_counter() - t0) / 20
	speed2 = len(samples) / dt_call2

	t0 = time.perf_counter()
	for _ in range(20):
		staged_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=outx, io0=0, mx1=mx1, mx2=mx2)
	dt_call3 = (time.perf_counter() - t0) / 20
	speed3 = len(samples) / dt_call3


	t3_expected = (len(samples) / speed2) + (len(samples)*(1/div) / speed1)
	speed3_expected = len(samples) / t3_expected

	print("Speed frac:         {} MS/s".format( round(1e-6*speed1, 3)))
	print("Speed div:          {} MS/s".format( round(1e-6*speed2, 3)))
	print("Speed staged:       {} MS/s".format( round(1e-6*speed3, 3)))
	print("Speed staged expc:  {} MS/s".format( round(1e-6*speed3_expected, 3)))
	print("io3:           {}".format(io3))
	print("io3 expec:     {}".format( r_rate*len(samples) ))

	waterfall_mx(samples=out2, fftlen=2048, fft_jump=1024, srate=1e6/div, plot_and_show=True, y_is_time=True)
	waterfall_mx(samples=out1-out2, fftlen=2048, fft_jump=1024, srate=1e6/div, plot_and_show=True, y_is_time=True)

	for i in range(halflen*2 +1):
		diff = out1[halflen*3:halflen*6] - out2[halflen*3+i:halflen*6+i]
		print("diff avg amp: ", np.average( np.abs(diff) ))



def interpolate(xarr, yarr, x):
	assert len(xarr) == len(yarr)
	for i0 in range(len(xarr)-1):
		if (x > xarr[i0]) and (x <= xarr[i0+1]):
			dx = x - xarr[i0]
			k = (yarr[i0+1] - yarr[i0]) / (xarr[i0+1] - xarr[i0])
			y_interp = yarr[i0] + k*dx
			return y_interp
	raise AssertionError("x coordinate not in given x-axis")


def interpolate2(xarr, yarr, x):
	assert len(xarr) == len(yarr)
	indexes = np.array( (0, len(xarr)//2, len(xarr)), dtype=np.int64)
	while (indexes[1] > indexes[0]) and (indexes[1] < indexes[2]):
		i = indexes[1]
		if (x > xarr[i]) and (x <= xarr[i+1]):
			dx = x - xarr[i]
			k = (yarr[i+1] - yarr[i]) / (xarr[i+1] - xarr[i])
			y_interp = yarr[i] + k*dx
			return y_interp
		if x > xarr[i]:
			if indexes[1] == (indexes[2]-1):
				raise AssertionError("x coordinate not in given x-axis")
			indexes[0] = indexes[1]
			indexes[1] = (indexes[1]+indexes[2])//2
		else:
			if indexes[1] == indexes[0]:
				raise AssertionError("x coordinate not in given x-axis")
			indexes[2] = indexes[1]
			indexes[1] = (indexes[0] + indexes[1]) // 2





def measure_differences(xarr0, yarr0, xarr1, yarr1, i1_from, i1_to):
	diff_array = np.zeros(len(xarr1), dtype=yarr0.dtype)
	for i1 in range(i1_from, i1_to+1):
		x = xarr1[i1]
		y_interp = interpolate(xarr=xarr0, yarr=yarr0, x=x)
		diff_array[i1] = yarr1[i1] - y_interp
	return diff_array






def test_2():
	x_arr = np.arange(2048*2)
	y_arr = np.sin(x_arr * 0.041) + np.cos(x_arr * 0.0111)*2   + np.cos(x_arr * 0.080112341)*0.2   + np.cos(x_arr * 0.12074512341)*0.3
	y_arr = y_arr * (1-np.cos(x_arr * np.pi*2/len(x_arr)))
	y_arr = np.array(y_arr, dtype=np.complex128)
	y2_arr = y_arr*0.0
	y3_arr = y_arr*0.0


	r_rate = 1/3.71
	rsmx = create_resampler(m_halflen=32, n_banks=64, r_rate=r_rate, f_cutoff=r_rate*0.499, allow_aliasing=False)
	io2 = resampler_execute_stream(in_arr=y_arr, ii0=0, nsamples=len(y_arr), out_arr=y2_arr, io0=0, statemx=rsmx)
	x_arr2 = (np.arange(io2)  / r_rate) - 32

	mx1,mx2 = create_staged_resampler(halflen_div=32, halflen_f=12, r_rate=r_rate, n_banks=64, f_cutoff=r_rate*0.499, allow_aliasing=False)
	io3 = staged_resampler_execute_stream(in_arr=y_arr, ii0=0, nsamples=len(y_arr), out_arr=y3_arr, io0=0, mx1=mx1, mx2=mx2)
	x_arr3 = (np.arange(io3) / r_rate) -32  -12/(1/int(1/r_rate))


	diff2 = measure_differences(xarr0=x_arr, yarr0=y_arr, xarr1=x_arr2, yarr1=y2_arr, i1_from=200, i1_to=len(x_arr2)-200)
	diff3 = measure_differences(xarr0=x_arr, yarr0=y_arr, xarr1=x_arr3, yarr1=y3_arr, i1_from=200, i1_to=len(x_arr3)-200)

	print("Avg diff 2:  {}".format( np.average( np.abs(diff2) )))
	print("Avg diff 3:  {}".format( np.average( np.abs(diff3) )))

	fig = plt.figure(figsize=(14,14))

	ax = fig.add_subplot(111)
	ax.plot(x_arr, y_arr.real)
	ax.plot(x_arr2, y2_arr[0:io2].real, marker="x")
	ax.plot(x_arr3, y3_arr[0:io3].real, marker="x")
	ax.grid()

	fig.set_layout_engine("tight")
	plt.show()




def test_3_speed():
	samples = radionoise(n=100000, sr=1e6, W_per_Hz=0.1/9600)
	out_arr = samples*0.0

	r_rate = 1/3.71
	halflen = 16

	rsmx = create_resampler(m_halflen=halflen, n_banks=64, r_rate=r_rate, f_cutoff=r_rate*0.499, allow_aliasing=False)
	io2 = resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out_arr, io0=0, statemx=rsmx)
	_ = resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out_arr, io0=0, statemx=rsmx)
	t0 = time.perf_counter()
	for _ in range(16):
		_ = resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out_arr, io0=0, statemx=rsmx)
	dt_unitary = (time.perf_counter() -t0) / 16
	speed_unitary = len(samples) / dt_unitary
	print("Classic speed (m={}):   {} MS/s".format(halflen, round(1e-6*speed_unitary, 3) ))

	speeds = list()
	for halflen1 in range(12,32):
		for halflen2 in range(12,32):
			print("Testing: {},{}".format(halflen1,halflen2))
			mx1,mx2 = create_staged_resampler(halflen_div=halflen1, halflen_f=halflen2, r_rate=r_rate, n_banks=64, f_cutoff=r_rate*0.499, allow_aliasing=False)
			_ = staged_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out_arr, io0=0, mx1=mx1, mx2=mx2)
			#_ = staged_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out_arr, io0=0, mx1=mx1, mx2=mx2)
			t0 = time.perf_counter()
			for _ in range(5):
				_ = staged_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out_arr, io0=0, mx1=mx1, mx2=mx2)
			dt_staged = (time.perf_counter() -t0) / 5
			speed_staged = len(samples) / dt_staged
			speeds.append(speed_staged)
	speeds = np.array(speeds)


	fig = plt.figure(figsize=(14,14))
	ax = fig.add_subplot(111)
	ax.plot(np.arange(len(speeds)), 1/speeds)
	ax.grid()
	fig.set_layout_engine("tight")
	plt.show()










def test_4_depict_filtering():
	samples = radionoise(n=1000000, sr=1e6, W_per_Hz=0.1/9600)
	out_arr1 = samples*0.0
	out_arr2 = samples*0.0

	r_rate = 1/4.5

	halflen11 = 14+5
	halflen12 = 14-5

	halflen21 = 14-5
	halflen22 = 14+5


	mx11,mx12 = create_staged_resampler(halflen_div=halflen11, halflen_f=halflen12, r_rate=r_rate, n_banks=64, f_cutoff=r_rate*0.499, allow_aliasing=False)
	mx21,mx22 = create_staged_resampler(halflen_div=halflen21, halflen_f=halflen22, r_rate=r_rate, n_banks=64, f_cutoff=r_rate*0.499, allow_aliasing=False)

	nout1 = staged_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out_arr1, io0=0, mx1=mx11, mx2=mx12)
	nout2 = staged_resampler_execute_stream(in_arr=samples, ii0=0, nsamples=len(samples), out_arr=out_arr2, io0=0, mx1=mx21, mx2=mx22)

	fft1, freqs1 = fft_usual(iq_arr=out_arr1[0:2048*4], srate=1e6*r_rate, take_abs=True)
	fft1 += fft_usual(iq_arr=out_arr1[2048*4:2048*8], srate=1e6*r_rate, take_abs=True)[0]

	fft2, freqs2 = fft_usual(iq_arr=out_arr2[0:2048*4], srate=1e6*r_rate, take_abs=True)
	fft2 += fft_usual(iq_arr=out_arr2[2048*4:2048*8], srate=1e6*r_rate, take_abs=True)[0]

	fft1 = rollsmooth(fft1, 200, 1)
	fft2 = rollsmooth(fft2, 200, 1)

	fig = plt.figure(figsize=(14,14))
	ax = fig.add_subplot(111)
	ax.plot(freqs1, fft1)
	ax.plot(freqs2, fft2)
	ax.grid()
	fig.set_layout_engine("tight")
	plt.show()






test_3_speed()
#test_4_depict_filtering()







































