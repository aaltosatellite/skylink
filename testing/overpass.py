import numpy as np
from matplotlib import pyplot as plt

Me = 5.972e24
Gg = 6.67408e-11
Re = 6374000.0
c_light = 3e8

def npv3(a,b,c):
	return np.array((a,b,c),dtype=np.float64)

def v_orbit(h):
	g = Gg*Me / ((Re+h)**2)
	v = (g*(Re+h))**0.5
	return v


"""
A class that calculates rough approximations to satellite distance from ground station at time t.
The satellite is on exact polar orbit, so inclination = 90 degrees.
"""
class Overpass:
	def __init__(self, GS_offset_deg, GS_lat_deg, H_orbit_m):
		"""
		:param GS_offset_deg:  	By how many degrees the initial orbit misses the ground station. 0 = directly overhead.
		:param GS_lat_deg: 		Latitude of ground station
		:param H_orbit_m: 		Orbit height
		"""
		self.gs_offset_rad 	= 2 * np.pi * GS_offset_deg / 360
		self.gs_lat_rad  	= 2 * np.pi * GS_lat_deg / 360
		self.h_orbit = H_orbit_m
		self.v_orbit = v_orbit(self.h_orbit)
		self.w_orbit = self.v_orbit / (self.h_orbit + Re)
		self.w_day = 2*np.pi / (24*3600)

	def d_at_t(self, t_offset):
		"""
		:param t_offset: 	Time before or after initial closest approach in seconds.
		:return: 			Distance in meters.
		"""
		r_gs_z = np.sin(self.gs_lat_rad) * Re
		r_gs_x = np.cos(self.gs_lat_rad) * np.sin(self.gs_offset_rad + self.w_day*t_offset) * Re
		r_gs_y = np.cos(self.gs_lat_rad) * np.cos(self.gs_offset_rad + self.w_day*t_offset) * Re
		r_gs = npv3(r_gs_x,r_gs_y,r_gs_z)

		r_sat_x = 0
		r_sat_z = np.sin(self.gs_lat_rad + self.w_orbit*t_offset) * (Re + self.h_orbit)
		r_sat_y = np.cos(self.gs_lat_rad + self.w_orbit*t_offset) * (Re + self.h_orbit)
		r_sat = npv3(r_sat_x, r_sat_y, r_sat_z)
		v = r_sat - r_gs
		d = np.linalg.norm(v, 2)
		return d

	def ping_at_t(self, t_offset):
		d = self.d_at_t(t_offset)
		return d / c_light

	def over_horizon(self, t_offset, degrees):
		r_gs_z = np.sin(self.gs_lat_rad) * Re
		r_gs_x = np.cos(self.gs_lat_rad) * np.sin(self.gs_offset_rad + self.w_day*t_offset) * Re
		r_gs_y = np.cos(self.gs_lat_rad) * np.cos(self.gs_offset_rad + self.w_day*t_offset) * Re
		r_gs = npv3(r_gs_x,r_gs_y,r_gs_z)

		r_sat_x = 0
		r_sat_z = np.sin(self.gs_lat_rad + self.w_orbit*t_offset) * (Re + self.h_orbit)
		r_sat_y = np.cos(self.gs_lat_rad + self.w_orbit*t_offset) * (Re + self.h_orbit)
		r_sat = npv3(r_sat_x, r_sat_y, r_sat_z)

		r_gs_sat = r_sat - r_gs

		dotp = np.dot(r_gs, r_gs_sat)
		mag_r_gs = np.linalg.norm(r_gs, 2)
		mag_r_sat = np.linalg.norm(r_gs_sat, 2)
		costheta = dotp / (mag_r_gs * mag_r_sat)
		theta = np.arccos(costheta)
		theta_deg = theta * 360 / (np.pi*2)
		return (theta_deg+degrees) < 90



if __name__ == '__main__':
	#print(np.pi*2*(35785e3+Re) / v_orbit(35785e3) )
	#print(24*3600)
	overpass = Overpass(GS_offset_deg=9, GS_lat_deg=60.188, H_orbit_m=500e3)
	DIST_VISIBLE = []
	DIST = []
	T_VISIBLE = []
	T = []
	for t in np.linspace(-60*60*12, 60*60*12*4, 10000):
		d_ = overpass.d_at_t(t)
		DIST.append(d_)
		T.append(t/(60))
		if overpass.over_horizon(t, 0):
			DIST_VISIBLE.append(d_)
			T_VISIBLE.append(t/(60))


	DIST = np.array(DIST)
	DIST_VISIBLE = np.array(DIST_VISIBLE)
	DISTkm = DIST/1000.0
	DIST_VISkm = DIST_VISIBLE/1000.0
	PING = DIST / c_light
	PING_VIS = DIST_VISIBLE / c_light
	PINGms = PING*1000.0
	PING_VISms = PING_VIS*1000.0

	plt.subplot(211)
	plt.title("distance")
	plt.plot(T, DISTkm)
	plt.scatter(T_VISIBLE, DIST_VISkm, marker=".", color="red")
	plt.grid()
	plt.ylabel("km")
	plt.xlabel("day")

	plt.subplot(212)
	plt.title("ping")
	plt.plot(T, PINGms)
	plt.scatter(T_VISIBLE, PING_VISms, marker="x", color="red")
	plt.grid()
	plt.ylabel("ms")
	plt.xlabel("day")
	plt.show()












