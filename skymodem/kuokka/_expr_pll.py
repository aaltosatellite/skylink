import numpy as np
from matplotlib import pyplot as plt
from mtools.tools_dsp import pll_cont_step, create_pll_statevector, winstd_init, pll_single_shot, pll_cont, pll_cont_strm
from numba import njit
import time
import pickle
from mtools.tools_system import colorcode01
from mtools.tools_math import npv3

def tst_measure_pll_delta_std_constant(NN):
	samples = np.random.normal(0,1,NN) + np.random.normal(0,1,NN)*1j

	sr   	= 1.0 + np.random.random()*10000
	c_freq 	= 0.1
	f_limit = 200.45*sr
	statev = create_pll_statevector(srate=sr, c_freq=c_freq, c_phase=c_freq**0.5, fmin=-f_limit, fmax=f_limit)
	f_arr = np.zeros(NN)
	p_arr = np.zeros(NN)
	io = 0
	for s in samples:
		pll_cont_step(sample=s, out_arr=f_arr, io0=io, statev=statev)
		p_arr[io] = statev[1]
		io += 1
	fd_arr = f_arr - np.roll(f_arr, 1)
	fd_arr[0] = 0
	std_arr_200, _ ,_ 	= winstd_init(arr=fd_arr, windowlen=200)
	std_arr_1200, _ ,_ 	= winstd_init(arr=fd_arr, windowlen=1200)


	msm_200 	= np.average(std_arr_200) / (c_freq*sr)
	msm_1200 	= np.average(std_arr_1200) / (c_freq*sr)
	msm_all 	= np.std(fd_arr) / (c_freq*sr)
	hypothesis 	= np.sqrt(np.pi) * c_freq * sr
	print("="*40)
	print("Measuring the PLL Df standard deviation on noise.")
	print("\tW=200:    {}".format(msm_200))
	print("\tW=1200:   {}".format(msm_1200))
	print("\tall:      {}".format(msm_all))
	print("")
	print("\tsqrt(PI)  {}".format(np.sqrt(np.pi)))
	print("="*40)
	print("")


	fig = plt.figure(figsize=(21,12))
	ax1 = fig.add_subplot(111)

	ax1.plot(np.arange(NN), std_arr_200)
	ax1.plot(np.arange(NN), std_arr_1200)
	ax1.plot(np.arange(NN), np.zeros(NN)+hypothesis, color="black", linestyle="--")
	ax1.plot(np.arange(NN), np.zeros(NN)+msm_200*c_freq*sr, color="blue", linestyle="--")
	ax1.plot(np.arange(NN), np.zeros(NN)+msm_1200*c_freq*sr, color="orange", linestyle="--")
	ax1.plot(np.arange(NN), np.zeros(NN)+msm_all*c_freq*sr, color="red", linestyle="--")
	ax1.grid()

	fig.set_layout_engine("tight")
	plt.show()



def tst_plot_zeroline_by_cf(NN, cf_arr, c_limit):
	samples = np.random.normal(0,1,NN) + np.random.normal(0,1,NN)*1j
	sr   	= 1.0 + np.random.random()*10000
	zeroline_arr = np.zeros(len(cf_arr))
	for ii, c_freq in enumerate(cf_arr):
		if (ii%5) == 0:
			print("{}/{}".format(ii, len(cf_arr)))
		f_limit = c_limit*sr
		statev = create_pll_statevector(srate=sr, c_freq=c_freq, c_phase=c_freq**0.5, fmin=-f_limit, fmax=f_limit)
		f_arr = np.zeros(NN)
		p_arr = np.zeros(NN)
		io = 0
		for s in samples:
			pll_cont_step(sample=s, out_arr=f_arr, io0=io, statev=statev)
			p_arr[io] = statev[1]
			io += 1
		fd_arr = f_arr - np.roll(f_arr, 1)
		fd_arr[0] = 0
		msm_all = np.std(fd_arr) / (c_freq*sr)
		zeroline_arr[ii] = msm_all

	f = open("cfarr_climit_zerolinearr_{}.dat".format(np.random.randint(100,999)), "wb")
	f.write( pickle.dumps( [cf_arr,c_limit,zeroline_arr] ) )
	f.close()

	fig = plt.figure(figsize=(16,14))
	ax1 = fig.add_subplot(111)
	ax1.plot(cf_arr/c_limit, zeroline_arr, marker="x")
	ax1.plot(cf_arr/c_limit, np.ones(len(cf_arr))*1.813, marker="x")
	ax1.plot(cf_arr/c_limit, fit26(c_freq=cf_arr, c_limit=c_limit) , marker="o")
	ax1.semilogx()
	ax1.set_title("standard deviation of the derivative of F-term, while f_limit=0.5")
	ax1.set_xlabel(r"c_freq / c_limit ($\frac{c_f}{c_{\text{limit}}}$)")
	ax1.set_ylabel(r"zeroline std($\Delta F$)")
	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()




def tst_pll_with_plotted_trace():
	print("-----------------------------------------------")
	print("PLL")
	# ====================
	c_freq 		= 0.001
	noiseamp 	= 1.0
	windowlen	= 300
	# ====================
	sps 		= 17
	NNsig 		= int(sps * 634)
	NNnoise 	= 40000
	sr 			= sps * 9600.0
	f_limit 	= 26e3
	f_sig 		= 10e3
	assert f_sig < (0.5*sr)
	samples =  np.exp(np.arange(NNsig)*2j*np.pi*f_sig/sr)
	end_noise = np.random.normal(0,1, NNnoise) + np.random.normal(0,1, NNnoise)*1j
	samples = np.concatenate( (end_noise, samples, end_noise) )
	NN = len(samples)
	samples = samples + noiseamp * (np.random.normal(0,1,NN) + 1j*np.random.normal(0,1,NN))


	statev = create_pll_statevector(srate=sr, c_freq=c_freq, c_phase=c_freq**0.5, fmin=-f_limit, fmax=f_limit)
	f_arr = np.zeros(NN)
	p_arr = np.zeros(NN)
	io = 0
	for s in samples:
		if io < NNnoise:
			#statev[0] = f_limit*1.0
			pass
		if io == NNnoise:
			#statev[0] = f_sig
			pass
		pll_cont_step(sample=s, out_arr=f_arr, io0=io, statev=statev)
		p_arr[io] = statev[1]
		io += 1
	fd_arr = f_arr - np.roll(f_arr, 1)
	fd_arr[0] = 0
	stdarr, _ ,_ = winstd_init(arr=fd_arr, windowlen=windowlen)
	zeroline = 1.81 * c_freq * sr

	fig = plt.figure(figsize=(21,12))
	ax1 = fig.add_subplot(311)
	ax2 = fig.add_subplot(312)
	ax3 = fig.add_subplot(313)

	ax1.plot(np.arange(NN), np.cos(p_arr))
	ax1.plot(np.arange(NN), samples.real)
	ax1.grid()

	ax2.plot(np.arange(NN), f_arr)
	ax2.plot([NNnoise,NNnoise], [-f_limit, f_limit], linestyle="--", color="black")
	ax2.plot([NNnoise+NNsig,NNnoise+NNsig], [-f_limit, f_limit], linestyle="--", color="black")
	ax2.set_ylabel(r"$F$ - [Hz]")
	ax2.grid()

	ax3.plot(np.arange(NN), stdarr)
	ax3.plot(np.arange(NN), np.zeros(NN)+zeroline)
	ax3.plot(np.arange(NN), np.zeros(NN)+zeroline*0.90, linestyle="--")
	ax3.plot(np.arange(NN), np.zeros(NN)+zeroline*0.80, linestyle="--")
	ax3.plot(np.arange(NN), np.zeros(NN)+zeroline*0.70, linestyle="--")
	ax3.plot([NNnoise,NNnoise], [zeroline*0.5, zeroline*1.1], linestyle="--", color="black")
	ax3.plot([NNnoise+NNsig,NNnoise+NNsig], [zeroline*0.5, zeroline*1.1], linestyle="--", color="black")
	ax3.set_ylabel(r"std( $\Delta F$ )")
	ax3.grid()

	fig.set_layout_engine("tight")
	plt.show()






@njit(cache=True)
def pll_slip_test(NN, A_noise, c_freq, f_sig, windowlen):
	assert -0.5 < f_sig < 0.5
	signal  = np.exp(np.arange(NN)*2j*np.pi*f_sig)
	samples = signal + A_noise * (np.random.normal(0,1, NN) + 1j*np.random.normal(0,1, NN))
	statev = create_pll_statevector(srate=1.0, c_freq=c_freq, c_phase=c_freq**0.5, fmin=-1000.5, fmax=1000.5) #note: fmin and fmax do not matter here, as we preset the pll to the correct frequency
	statev[0] = f_sig
	f_arr = pll_cont(samples=samples, statev=statev)
	df_arr = f_arr - np.roll(f_arr, 1)
	df_arr[0] = df_arr[1]
	std_arr, _, _ = winstd_init(df_arr, windowlen=windowlen)
	std_zeroline = 1.812 * c_freq * 1.0
	over_line = std_arr > std_zeroline
	over_line_amax = np.argmax(over_line)
	if not over_line[over_line_amax]:
		return -1
	return over_line_amax


def obtain_slip_array(NN, Anoise, nRep, c_freq, f_sig, windowlen):
	p_arr = np.zeros( NN )
	t0 = time.perf_counter()
	for _ in range(nRep):
		slip_idx = pll_slip_test(NN=NN, A_noise=Anoise, c_freq=c_freq, f_sig=f_sig, windowlen=windowlen)
		if slip_idx >= 0:
			p_arr[slip_idx:] += 1/nRep
	dT = (time.perf_counter() - t0)
	dt_per = dT / nRep
	print("dT:        {} s".format( round(dT, 3)))
	print("dt per:    {} ms".format( round(1e3*dt_per, 2)))
	return p_arr



def obtain_slip_matrix(NN, Anoise_arr, nRep, c_freq, f_sig, windowlen):
	nA = len(Anoise_arr)
	mx = np.zeros( (NN,nA) )
	t0 = time.perf_counter()
	for iA in range(nA):
		A = Anoise_arr[iA]
		for _ in range(nRep):
			slip_idx = pll_slip_test(NN=NN, A_noise=A, c_freq=c_freq, f_sig=f_sig, windowlen=windowlen)
			if slip_idx >= 0:
				mx[slip_idx:,iA] += 1/nRep
	dT = (time.perf_counter() - t0)
	dt_per = dT / (nA*nRep)
	print("dT:        {} s".format( round(dT, 3)))
	print("dt per:    {} ms".format( round(1e3*dt_per, 2)))
	return mx





def tst_draw_Pslip_in_2D_matrix():
	nA = 32
	NN = 17*634*4
	nRep = 400

	Anoise_arr = np.linspace(0, 1.5, nA)
	mx = obtain_slip_matrix(NN=NN, Anoise_arr=Anoise_arr, nRep=nRep, c_freq=0.001, f_sig=0.0, windowlen=200)

	extent = (Anoise_arr[0],Anoise_arr[-1], 0,NN)
	aspect = (extent[1]-extent[0]) / (extent[3]-extent[2])
	fig = plt.figure(figsize=(13,13))
	ax1 = fig.add_subplot(111)

	ax1.imshow(mx, origin="lower",  extent=extent, aspect=aspect)
	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()






def tst_unlock_slip():
	NN = 17*634*4
	nRep = 400


	xx = np.arange(NN)*1.0
	lines_cf = list()
	c_freq_arr = np.geomspace(0.0001, 0.01, 8)
	for c_freq in c_freq_arr:
		p_arr = obtain_slip_array(NN=NN, Anoise=1.0, nRep=nRep, c_freq=c_freq, f_sig=0.0, windowlen=200)

		best_expn = 0
		expn_arr = np.linspace(0.0,1.0, 32)
		dx = expn_arr[1]-expn_arr[0]
		for _ in range(16):
			sq_errs = [np.sum(((1 - expn**(xx/100)) - p_arr)**2) for expn in expn_arr]
			best_idx = np.argmin(sq_errs)
			best_expn = expn_arr[best_idx]
			expn_arr = np.linspace( max(0,best_expn-dx*3), min(best_expn+dx*3, 1), 32)
			dx = expn_arr[1]-expn_arr[0]

		lines_cf.append([p_arr, c_freq, best_expn])



	"""lines_fsig = list()
	for f_sig in np.linspace(0.0, 0.46, 16):
		p_arr = obtain_slip_array(NN=NN, Anoise=1.0, nRep=nRep, c_freq=0.001, f_sig=f_sig, windowlen=200)
		lines_fsig.append([p_arr, f_sig])"""


	fig = plt.figure(figsize=(18,13))
	ax1 = fig.add_subplot(121)
	ax2 = fig.add_subplot(122)

	for i, (p_arr,c_freq,exproot) in enumerate(lines_cf):
		#color = npv3(0,0,1)*(1 - (i/len(lines_cf)))  + npv3(1,0,0)*(i/len(lines_cf))
		ax1.plot(np.arange(NN), p_arr, label="c_freq = "+str(c_freq)) #color=colorcode01(*color )
	for i, (p_arr,c_freq,exproot) in enumerate(lines_cf):
		ax1.plot(xx, 1 - exproot**(xx/100))

	ax1.grid()
	ax1.legend()

	root_arr = [x[2] for x in lines_cf]
	ax2.plot(c_freq_arr, root_arr, marker="x")
	#ax2.semilogx()
	#ax2.semilogy()
	ax2.set_xlabel("$c_f$")
	ax2.grid()

	#ax2 = fig.add_subplot(122)
	"""for p_arr,f_sig in lines_fsig:
		ax2.plot(np.arange(NN), p_arr, label="f_sig = "+str(f_sig))
	ax2.grid()
	ax2.legend()"""

	fig.set_layout_engine("tight")
	plt.show()




def fit26(c_freq, c_limit):
	#if c_freq/c_limit < 0.003:
	#	return 1.813
	coeffs = [ -6.588834003638643e-10, -2.5411768833287384e-09, 4.981408860432077e-08, 1.868537663753007e-07, -1.6876213332527186e-06, -6.0927590116583276e-06, 3.381339596106427e-05,
			0.00011601629871765534, -0.0004452159771682212, -0.001429960454287664, 0.0040467863424894085, 0.011952688145626354, -0.025947547880166965, -0.06904834881024477, 0.11779091026188404,
			0.2760086678268451, -0.37435257361191565, -0.7542160313297891, 0.8113440477869422, 1.3790490653091885, -1.1482083879741865, -1.6614950216935542, 0.9964447101954652,
			1.4083888509528601, -0.4914604337344185, -1.3722975909449475, 1.033787593289162,
			]
	x = np.log10(c_freq/c_limit)
	y = 0
	for ip in range(27):
		y += coeffs[ip] * x**(27-(ip+1))
	y = 1.813*(x < np.log10(0.003)) + y*(np.log10(0.003) <= x)*(x < 3) + 0.0*(3 <= x)
	return y




def sigm1(x):
	return 1 - 1/ (1 + np.exp( -x ))

def sigm2(x):
	return 0.5 - 0.5*x/ np.sqrt(1+x**2)

def sigm3(x):
	return 0.5 - 0.5*x/ (1+np.abs(x))

def sigm4(x):
	return 0.5 - 0.5*np.tanh(x)


def fit1(f_idx, c_freq, c_limit, x0, c0, c1):
	xprime = c0*(np.log10(c_freq / c_limit) - x0)
	f = [sigm1, sigm2, sigm3, sigm4][f_idx]
	return c1*f(x=xprime)


def poly_use(x_arr, p_arr):
	y_arr = np.zeros(len(x_arr))
	n_poly = len(p_arr)
	for ix in range(len(x_arr)):
		y = 0.0
		for ip in range(n_poly):
			y += p_arr[ip] * x_arr[ix]**(n_poly-(ip+1))
		y_arr[ix] = y
	return y_arr



#c00_list = [2.7836, 1.6702, 4.5792, 1.3918]
#err_list = [0.018603098988782, 0.0402577692, 0.5583038, 0.01860309]

from numpy import polyfit

def fit_zeroline_trend(file_int):
	f = open("cfarr_climit_zerolinearr_{}.dat".format(file_int), "rb")
	rd = f.read()
	f.close()
	[cf_arr,c_limit,zeroline_arr] = pickle.loads(rd)
	assert len(cf_arr) == len(zeroline_arr)
	nn = len(zeroline_arr)
	xx = cf_arr/c_limit
	log_xx = np.log10(xx)
	Z0 = 1.813
	x00 = np.log10(1.220)


	polyfit1 = polyfit(x= np.log10(cf_arr/c_limit), y = zeroline_arr, deg=10)
	polyfit2 = polyfit(x= np.log10(cf_arr/c_limit), y = zeroline_arr, deg=15)
	polyfit3 = polyfit(x= np.log10(cf_arr/c_limit), y = zeroline_arr, deg=26)
	print("[")
	for coeff in polyfit3:
		print("{}, ".format(coeff))
	print("]")
	y_poly_1 = poly_use(x_arr=log_xx, p_arr=polyfit1)
	y_poly_2 = poly_use(x_arr=log_xx, p_arr=polyfit2)
	y_poly_3 = poly_use(x_arr=log_xx, p_arr=polyfit3)

	print("poly error 1: {}".format( np.sum( (y_poly_1 - zeroline_arr)**2 ) ))
	print("poly error 2: {}".format( np.sum( (y_poly_2 - zeroline_arr)**2 ) ))
	print("poly error 3: {}".format( np.sum( (y_poly_3 - zeroline_arr)**2 ) ))


	c0_list = [2.6, 1.6, 2.5, 1.4]
	c1_list = [Z0, Z0, Z0, Z0]
	err_list = [10000,]*4
	for f_idx in range(4):
		c0 = c0_list[f_idx]
		c1 = c1_list[f_idx]
		err = np.sum((fit1(f_idx=f_idx, c_freq=cf_arr, c_limit=c_limit, x0=x00, c0=c0, c1=c1) - zeroline_arr)**2)
		for _ in range(100000):
			c0_ = c0 + np.random.normal(0, 0.1)
			c1_ = c1 + np.random.normal(0, 0.1)
			err_ = np.sum((fit1(f_idx=f_idx, c_freq=cf_arr, c_limit=c_limit, x0=x00, c0=c0_, c1=c1_) - zeroline_arr)**2)
			err_list[f_idx] = err
			if err_ < err:
				err = err_
				c0 = c0_
				c1 = c1_
				err_list[f_idx] = err_
				print("New best f-{}:".format(f_idx+1))
				print("\tc0 = {}:".format(c0))
				print("\tc1 = {}:".format(c1))
		c0_list[f_idx] = c0
		c1_list[f_idx] = c1
	print("New c0 list:")
	[print(round(c0, 3), end=", ") for c0 in c0_list]
	print("")
	[print(round(c1, 3), end=", ") for c1 in c1_list]
	print("")
	[print(round(err, 4), end=", ") for err in err_list]
	print("")


	fig = plt.figure(figsize=(17,11))
	ax1 = fig.add_subplot(111)
	ax1.plot(xx, zeroline_arr, marker="x")
	ax1.plot(xx, np.ones(nn)*Z0)
	ax1.plot(xx, np.ones(nn)*Z0*0.5)
	ax1.plot(xx, fit1(f_idx=0, c_freq=cf_arr, c_limit=c_limit, x0=x00, c0=c0_list[0], c1=c1_list[0]), label="1")
	#ax1.plot(xx, fit1(f_idx=1, c_freq=cf_arr, c_limit=c_limit, x0=x00, c0=c0_list[1], c1=c1_list[1]), label="2")
	#ax1.plot(xx, fit1(f_idx=2, c_freq=cf_arr, c_limit=c_limit, x0=x00, c0=c0_list[2], c1=c1_list[2]), label="3")

	#ax1.plot(xx, y_poly_10)
	#ax1.plot(xx, y_poly_15)
	#ax1.plot(xx, y_poly_3, marker="x")

	ax1.plot(xx, fit26(c_freq=cf_arr, c_limit=c_limit), marker="x")

	ax1.legend()
	ax1.semilogx()
	ax1.grid()
	fig.set_layout_engine("tight")
	plt.show()




















if __name__ == '__main__':
	#fit_zeroline_trend(file_int=659)

	#tst_measure_pll_delta_std_constant(NN=1000000)
	tst_plot_zeroline_by_cf(NN=500000, cf_arr=np.geomspace(0.0001, 1000.0, 50), c_limit=0.5)
	#tst_pll_with_plotted_trace()
	#tst_unlock_slip()





