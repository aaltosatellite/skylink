import numpy as np





def unite_scopes(scope1, scope2):
	return min(scope1[0], scope2[0]), max(scope1[1], scope2[1])



class PlotLine:
	def __init__(self, n_retain, x_interval=1.0, logs=(False,False)):
		self.xlog = logs[0]
		self.ylog = logs[1]
		self.ydata = np.linspace(0,1,n_retain) * 0.0
		self.xdata = np.linspace(0, x_interval*n_retain, n_retain)

	def add_y(self, y):
		self.ydata = np.roll(self.ydata, -1)
		self.ydata[-1] = y

	def add_xy(self, x, y):
		self.ydata = np.roll(self.ydata, -1)
		self.ydata[-1] = y
		self.xdata = np.roll(self.xdata, -1)
		self.xdata[-1] = x

	def cast_y_to_coordinates(self, origin_y, pix_per_y, origin_pix_y):
		y = self.ydata.copy()
		if self.ylog:
			y = np.log10(y)
		y = y - origin_y
		return y * pix_per_y + origin_pix_y

	def cast_x_to_coordinates(self, origin_x, pix_per_x, origin_pix_x):
		x = self.xdata.copy()
		if self.xlog:
			x = np.log10(x)
		x = x - origin_x
		return x * pix_per_x + origin_pix_x

	def get_scope(self, prior_scope=(None,None), top_margin=0.0):
		xscope = self.get_x_scope(prior_scope[0])
		yscope = self.get_y_scope(prior_scope[1], top_margin)
		return xscope, yscope

	def get_y_scope(self, prior_scope=None, top_margin=0.0):
		y = self.ydata.copy()
		if self.ylog:
			y = np.log10(y)
		scope = np.min(y), np.max(y)

		#(y1-y0) / (1-top)  + y0 = y2

		d = scope[1] - scope[0]
		y2 = d/(1-top_margin) + scope[0]
		scope = scope[0], y2
		if prior_scope:
			scope = unite_scopes(scope, prior_scope)
		return scope

	def get_x_scope(self, prior_scope=None):
		x = self.xdata.copy()
		if self.xlog:
			x = np.log10(x)
		scope = np.min(x), np.max(x)
		if prior_scope:
			scope = unite_scopes(scope, prior_scope)
		return scope



def get_gridlines(n_pref, scope):
	span = scope[1] - scope[0]
	if span == 0:
		span_order = 1
	else:
		span_order = int(np.log10(span))
	smallest_error = 10000000
	divisor = 1
	for base_divisor_candidate in (1,2,5):
		for exponent_candidate in range(span_order-2,span_order+2):
			divisor_candidate = base_divisor_candidate * 10**exponent_candidate
			error = abs(n_pref - span/divisor_candidate)
			if error < smallest_error:
				smallest_error = error
				divisor = divisor_candidate
				#base_divisor = base_divisor_candidate
				#exponent = exponent_candidate
	values = []
	i0 = int(np.floor(scope[0] / divisor))
	i1 = int(np.ceil(scope[1] / divisor))
	for i in range(i0, i1+1):
		values.append(i*divisor)

	return np.array(values, dtype=np.float64)











