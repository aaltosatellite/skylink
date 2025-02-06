import numpy as np
from matplotlib import pyplot as plt
from kuokka.lib_tools import make_samples


def rollsmooth(arr, n):
	arr_ = arr.copy()
	for _ in range(n):
		arr_ = (np.roll(arr_, 1) + np.roll(arr_, -1) + arr_) * (1/3)
	return arr_

def radionoise(n, sr, W_per_Hz):
	"""
	:param n: number of samples
	:param sr: samplerate
	:param W_per_Hz: spectral power density (Watts per Hertz)
	:return: n samples of normal distributed IQ noise
	To verify signal energy, and spectral power density:
		with df = sr/n
		sum((fft(samples)*df)**2) ≈ W_per_Hz * sr

		- Each fft-bin is (2*f_Nyquist) / n  Hertz wide.
		- fft bins represent amplitudes of constituent component frquencies.
	"""
	cc = (0.5*W_per_Hz*sr)**0.5
	return cc * (np.random.normal(0,1.0, n) + 1j*np.random.normal(0, 1.0, n))




bits_1 = np.random.randint(0,2,100)*2 -1
bits_2 = np.random.randint(0,2,300)*2 -1

baudrate = 9600 // 1
sps1 = 17
sps2 = 91
sr1 = baudrate * sps1
sr2 = baudrate * sps2
T1 = 1/sr1
T2 = 1/sr2

signal_1 = make_samples(sps_f=sps1, bitstring=bits_1, f_offset=0e3/sr1, power=1, mod_index=0.5, shaper_mode=0, shaper_BT_prod=0.8, shaper_n_taps=int(sps1*6)+1)
signal_2 = make_samples(sps_f=sps2, bitstring=bits_2, f_offset=0e3/sr2, power=1, mod_index=0.5, shaper_mode=0, shaper_BT_prod=0.8, shaper_n_taps=int(sps2*6)+1)
signal_1 = signal_1
signal_2 = signal_2

df1 = sr1 / len(signal_1)
df2 = sr2 / len(signal_2)
print("df1: {}".format(df1))
print("df2: {}".format(df2))


#P_per_Hz 	= 0.33 * (1/baudrate)**0.5
P_per_Hz 	= 0.2 / 9600
#cc1 		= T1 * (0.5*P_per_Hz*sr1)**0.5
#cc2 		= T2 * (0.5*P_per_Hz*sr2)**0.5
#noise_1 	= cc1 * (np.random.normal(0, 1.0, len(signal_1)) + 1j * np.random.normal(0, 1.0, len(signal_1)))
#noise_2 	= cc2 * (np.random.normal(0, 1.0, len(signal_2)) + 1j * np.random.normal(0, 1.0, len(signal_2)))

noise_1 = radionoise(n=len(signal_1), sr=sr1, W_per_Hz=P_per_Hz)
noise_2 = radionoise(n=len(signal_2), sr=sr2, W_per_Hz=P_per_Hz)



fft_s1 = np.abs(np.fft.fftshift(np.fft.fft(signal_1))) #/ len(samples_1) #/ df1  #/ len(samples_1) #/ T1
fft_s2 = np.abs(np.fft.fftshift(np.fft.fft(signal_2))) #/ len(samples_2) #/ df2  #/ len(samples_2) #/ T2

fft_n1 = np.abs( np.fft.fftshift( np.fft.fft(noise_1) ) ) #/ len(samples_1) #/ df1  #/ len(samples_1) #/ T1
fft_n2 = np.abs( np.fft.fftshift( np.fft.fft(noise_2) ) ) #/ len(samples_2) #/ df2  #/ len(samples_2) #/ T2


print("signal1 Sum:  {}".format(np.sum(fft_s1**2 * df1**2)))
print("signal2 Sum:  {}".format(np.sum(fft_s2**2 * df2**2)))
print("")

print("noise1 Sum:  {}".format(np.sum(fft_n1**2 * df1 * T1/len(noise_1))))
print("          vs {} predicted".format(sr1 * P_per_Hz))
print("")

print("noise2 Sum:  {}".format(np.average(fft_n2**2 * T2/(len(noise_2)))))
print("          vs {} predicted".format(P_per_Hz))
print("")

#  x * T2 / len ==

summed_1 = (signal_1 + noise_1) #* T1
summed_2 = (signal_2 + noise_2) #* T2
fft1 = np.abs( np.fft.fftshift( np.fft.fft(summed_1) ) )**2 *(T1/len(summed_1))**1.0
fft2 = np.abs( np.fft.fftshift( np.fft.fft(summed_2) ) )**2 *(T2/len(summed_2))**1.0

fft1 = rollsmooth(fft1, 10)
fft2 = rollsmooth(fft2, 10)
freqs1 = np.fft.fftshift(np.fft.fftfreq( len(fft1), d=(1/sr1) ))
freqs2 = np.fft.fftshift(np.fft.fftfreq( len(fft2), d=(1/sr2) ))




fig = plt.figure(figsize=(16,13))

ax = fig.add_subplot(111)

ax.plot(freqs1, fft1)
ax.plot(freqs2, fft2)
ax.grid()

fig.set_layout_engine("tight")
plt.show()










