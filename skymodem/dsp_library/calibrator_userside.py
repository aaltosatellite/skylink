import numpy as np
import time, os, sys





class Calibrator:
	def __init__(self, f_carrier_expected, integration_halfwidth_Hz):
		self.f_carrier0 = f_carrier_expected
		self.integration_halfwidth_Hz = integration_halfwidth_Hz
		pass


	def give_fftstack_array(self, fft_abs_stack, freq_array, f_tune):
		assert len(fft_abs_stack) == len(freq_array)
		assert fft_abs_stack.dtype in (np.float64, np.float32, np.float128)
		assert freq_array[0] < 0
		assert freq_array[-1] > 0
		assert np.isclose(freq_array[1]-freq_array[0] , freq_array[2] - freq_array[1])
		assert (freq_array[0]+f_tune) < self.f_carrier0
		assert (freq_array[-1]+f_tune) > self.f_carrier0
		df = freq_array[1] - freq_array[0]
		halfwidth_nbins = self.integration_halfwidth_Hz / df
		carrier_f_index = np.argmin( np.abs(freq_array+f_tune-self.f_carrier0) )
		assert carrier_f_index > halfwidth_nbins
		assert carrier_f_index < (len(freq_array) - (halfwidth_nbins+1))
		i_sum0 = carrier_f_index - halfwidth_nbins
		i_sum1 = carrier_f_index + halfwidth_nbins
		energy = np.sum( fft_abs_stack[i_sum0:i_sum1] )










