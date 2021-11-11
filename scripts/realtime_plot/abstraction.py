import numpy as np





def unite_partscopes(scope1, scope2):
	if scope1 is None:
		return scope2
	if scope2 is None:
		return scope1
	return min(scope1[0], scope2[0]), max(scope1[1], scope2[1])

def unite_scopes(scope1, scope2):
	return unite_partscopes(scope1[0], scope2[0]), unite_partscopes(scope1[1], scope2[1])



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





class YVector:
	def __init__(self, n_retain, log=False):
		self.ylog = log
		self.ydata = np.linspace(0,1,n_retain) * 0.0

	def add_y(self, y):
		self.ydata = np.roll(self.ydata, -1)
		self.ydata[-1] = y

	def cast_y_to_coordinates(self, origin_y, pix_per_y):
		y = self.ydata.copy()
		if self.ylog:
			y = np.log10(y)
		y = y - origin_y
		return y * pix_per_y

	def get_y_partscope(self, prior_yscope=None, top_margin=0.0):
		y = self.ydata.copy()
		if self.ylog:
			y = np.log10(y)
		partscope = np.min(y), np.max(y)
		d = partscope[1] - partscope[0]
		y2 = d/(1-top_margin) + partscope[0]
		partscope = partscope[0], y2
		if prior_yscope:
			partscope = unite_partscopes(partscope, prior_yscope)
		return partscope







class XYVector:
	def __init__(self, n_retain, logs=(False,False)):
		self.n_retain = n_retain
		self.xlog = logs[0]
		self.ylog = logs[1]
		self.ydata = np.array([], dtype=np.float64)
		self.xdata = np.array([], dtype=np.float64)

	def add_xy(self, x, y):
		self.xdata = np.append(self.xdata, x)[-self.n_retain:]
		self.ydata = np.append(self.ydata, y)[-self.n_retain:]

	def cast_to_coordinates(self, xfrom, xto, origin_x, pix_per_x,  origin_y, pix_per_y):
		indexes = (self.xdata >= xfrom) * (self.xdata <= xto)
		X = self.xdata[indexes]
		if self.xlog:
			X = np.log10(X)
		Y = self.ydata[indexes]
		if self.ylog:
			Y = np.log10(Y)
		X = X - origin_x
		Y = Y - origin_y
		return X*pix_per_x, Y*pix_per_y

	def get_scope(self, prior_scope=(None,None), top_margin=0.0):
		xscope = self.get_x_partscope(prior_scope[0])
		yscope = self.get_y_partscope(xscope, prior_scope[1], top_margin)
		return xscope, yscope

	def get_y_partscope(self, xscope, prior_partscope=None, top_margin=0.0):
		y = self.ydata.copy()
		y = y[(self.xdata>=xscope[0]) * (self.xdata<=xscope[1])]
		if len(y) == 0:
			return prior_partscope
		if self.ylog:
			y = np.log10(y)
		partscope = np.min(y), np.max(y)
		d = partscope[1] - partscope[0]
		y2 = d/(1-top_margin) + partscope[0]
		partscope = partscope[0], y2
		if prior_partscope:
			partscope = unite_partscopes(partscope, prior_partscope)
		return partscope

	def get_x_partscope(self, prior_partscope=None):
		x = self.xdata.copy()
		if len(x) == 0:
			return prior_partscope
		if self.xlog:
			x = np.log10(x)
		partscope = np.min(x), np.max(x)
		if prior_partscope:
			partscope = unite_partscopes(partscope, prior_partscope)
		return partscope













