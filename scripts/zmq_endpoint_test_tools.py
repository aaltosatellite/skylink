import os
import random
import time
import struct
import threading
import numpy as np
from zmq_endpoint import SkyLink
import zmq
from queue import Queue
from realtime_plot.drawer import create_realplot
from types import SimpleNamespace
from test_tools import corrupt
rint = random.randint
rng = random.random

NS = SimpleNamespace()
NS.TIME_RATE    = 1.0
NS.CORRUPT_RATE = 0.01
NS.LOSS_RATE    = 0.01
NS.DIST_LAG     = 0.010
NS.USE_GLOBALS  = False


def rsleep(x):
	time.sleep(x / NS.TIME_RATE)

def rtime():
	return time.perf_counter() * NS.TIME_RATE


def print_dict(dd:dict, lvl:int=0):
	prefix = "\t" * lvl
	sep = "="*30
	if lvl > 0:
		sep = "-"*30
	print(prefix+sep)
	for k in dd.keys():
		v = dd[k]
		if type(v) == dict:
			print(prefix + "{}::".format(k))
			print_dict(v, lvl+1)
		if (type(v) == list) and not (False in [type(x) == dict for x in v]):
			for mem in v:
				print_dict(mem, lvl+1)
		else:
			print(prefix + "{}: {}".format(k,v))
	print(prefix+sep)


class SkyPHYConfig:
	enable_scrambler = 1
	enable_rs = 1

	def dump(self):
		k = struct.pack("BB", self.enable_scrambler, self.enable_rs)
		return k

class SkyMACConfig:
	maximum_window_length_ticks = 2500
	minimum_window_length_ticks = 250
	gap_constant_ticks = 500
	tail_constant_ticks = 65
	shift_threshold_ticks = 5000
	idle_timeout_ticks = 25000
	window_adjust_increment_ticks = 250
	carrier_sense_ticks = 250
	unauthenticated_mac_updates = 0
	window_adjustment_period = 2
	idle_frames_per_window = 1

	def dump(self):
		members = [self.maximum_window_length_ticks, self.minimum_window_length_ticks, self.gap_constant_ticks]
		members.extend([self.tail_constant_ticks, self.shift_threshold_ticks, self.idle_timeout_ticks])
		members.extend([self.window_adjust_increment_ticks, self.carrier_sense_ticks, self.unauthenticated_mac_updates])
		members.extend([self.window_adjustment_period, self.idle_frames_per_window])
		d = struct.pack("6i2hBbB",*members)
		return d


class SkyVCConfig:
	element_size = 400
	rcv_ring_len = 36
	horizon_width = 24
	send_ring_len = 36
	require_authentication = 0b110

	def dump(self):
		members = [self.element_size, self.rcv_ring_len, self.horizon_width, self.send_ring_len, self.require_authentication]
		d = struct.pack("4iB", *members)
		return d

class HMACConfig:
	key_length = 16
	maximum_jump = 30
	key = b"1234567890abcdef"

	def dump(self):
		return struct.pack("ii16s", self.key_length, self.maximum_jump, self.key)


class SkyConfig:
	identity = b"\00" * 6
	arq_timeout_ticks = 15000
	arq_idle_frame_threshold = 15000 // 4
	arq_idle_frames_per_window = 1
	def __init__(self):
		self.phy = SkyPHYConfig()
		self.mac = SkyMACConfig()
		self.hmac = HMACConfig()
		self.vc = [SkyVCConfig(),SkyVCConfig(),SkyVCConfig(),SkyVCConfig()]
		self.vc[2].require_authentication = 0
		self.vc[3].require_authentication = 0

	def dump(self):
		b = b""

		b += self.phy.dump()
		b += b"\x00" * 2

		b += self.mac.dump()
		b += b"\x00" * 1

		b += self.hmac.dump()
		b += b"\x00" * 0

		for vc in self.vc:
			b += vc.dump()
			b += b"\x00" * 3

		b = b + self.identity
		b += b"\x00" * 2

		b += struct.pack("i",self.arq_timeout_ticks)
		b += b"\x00" * 0

		b += struct.pack("i",self.arq_idle_frame_threshold)
		b += b"\x00" * 0

		b += struct.pack("B",self.arq_idle_frames_per_window)

		b += (160-len(b)) * b"\00"
		return b



def corrupt_n_transmit(pl:bytes,
					   rx_socket:zmq.Socket,
					   socket_lock:threading.RLock,
					   id_target:int,
					   lag:float,
					   corrupt_rate:float,
					   loss_rate:float):
	if NS.USE_GLOBALS:
		if rng() < NS.LOSS_RATE:
			return
		plc = corrupt(pl, NS.CORRUPT_RATE)
		rsleep(NS.DIST_LAG)
	else:
		if rng() < loss_rate:
			return
		plc = corrupt(pl, corrupt_rate)
		rsleep(lag)
	tx = struct.pack("i", id_target) + plc
	with socket_lock:
		rx_socket.send(tx)



def transfer_loop(txsock, rxsock, idd_map, lista, lag, corrupt_rate, loss_rate):
	cc = 0
	txsock.setsockopt(zmq.RCVTIMEO, 200)
	socket_lock = threading.RLock()
	print("Transfer loop started.", flush=True)
	while True:
		cc += 1
		r = b""
		try:
			r = txsock.recv()
		except:
			pass
		if len(r) <= 4:
			continue
		idd = struct.unpack("i", r[0:4])[0]
		pl = r[4:]
		pair_idd = idd_map[idd]
		lista.append( (rtime(), idd, pl) )
		arguments = (pl, rxsock, socket_lock, pair_idd, lag, corrupt_rate, loss_rate)
		th = threading.Thread(target=corrupt_n_transmit, args=arguments)
		th.start()


#=== PLOTTING LOOPS ===============================================================================================
def plot_txrx(skylink1:SkyLink, skylink2:SkyLink):
	plot_que = create_realplot(14, 24, (2,2,0))
	rs1_ex = skylink1.get_status().radio_state
	rs2_ex = skylink2.get_status().radio_state
	plot_que.put( ("1-tx", rs1_ex) )
	plot_que.put( ("2-tx", rs2_ex) )
	while True:
		status1 = skylink1.get_status()
		status2 = skylink2.get_status()
		if status1.radio_state != rs1_ex:
			plot_que.put( ("1-tx", status1.radio_state) )
			rs1_ex = status1.radio_state
		if status2.radio_state != rs2_ex:
			plot_que.put( ("2-tx", status2.radio_state) )
			rs2_ex = status2.radio_state
		time.sleep(0.02)



def plot_remaining(skylink1:SkyLink, skylink2:SkyLink):
	plot_que = create_realplot(14, 24, (2,2,1))
	while True:
		status1 = skylink1.get_status()
		status2 = skylink2.get_status()
		rem1 = status1.own_remaining
		rem2 = status2.own_remaining
		plot_que.put( ("SL1 own remaining", rem1) )
		plot_que.put( ("SL2 own remaining", rem2) )
		plot_que.put( ("SL1 window", status1.my_window) )
		plot_que.put( ("SL2 window", status2.my_window) )
		plot_que.put( ("SL1 since last update", status1.tick - status1.last_mac_update) )
		plot_que.put( ("SL2 since last update", status2.tick - status2.last_mac_update) )
		plot_que.put( ("SL1 window_discrepancy", status1.peer_window - status2.my_window) )
		plot_que.put( ("SL2 window_discrepancy", status2.peer_window - status1.my_window) )
		plot_que.put( ("SL1 remaining_discrepancy", 2000+ (status1.own_remaining - status2.peer_remaining)) )
		plot_que.put( ("SL2 remaining_discrepancy", 2000+ (status2.own_remaining - status1.peer_remaining)) )
		time.sleep(0.09)



def plot_hmac(skylink1:SkyLink, skylink2:SkyLink):
	plot_que = create_realplot(14, 24, (2,2,2))
	while True:
		status1 = skylink1.get_status()
		status2 = skylink2.get_status()
		plot_que.put( ("VC0 1to2 hmac_delta", status1.vcs[0].hmac_tx_seq - status2.vcs[0].hmac_rx_seq ) )
		plot_que.put( ("VC0 2to1 hmac_delta", status2.vcs[0].hmac_tx_seq - status1.vcs[0].hmac_rx_seq ) )
		plot_que.put( ("SL1 VC0 hmac_rx % 10", status1.vcs[0].hmac_rx_seq % 10) )
		plot_que.put( ("SL2 VC0 hmac_rx % 10", status2.vcs[0].hmac_rx_seq % 10) )
		time.sleep(0.08)



def plot_arq(skylink1:SkyLink, skylink2:SkyLink):
	plot_que = create_realplot(14, 24, (2,2,3))
	while True:
		status1 = skylink1.get_status()
		status2 = skylink2.get_status()
		plot_que.put( ("SL1 VC0 ARQ", status1.vcs[0].arq_state ) )
		plot_que.put( ("SL1 VC1 ARQ", status1.vcs[1].arq_state +0.1) )
		plot_que.put( ("SL1 VC2 ARQ", status1.vcs[2].arq_state +0.2) )
		plot_que.put( ("SL1 VC3 ARQ", status1.vcs[3].arq_state +0.3) )
		plot_que.put( ("SL2 VC0 ARQ", status2.vcs[0].arq_state +10) )
		plot_que.put( ("SL2 VC1 ARQ", status2.vcs[1].arq_state +10.1) )
		plot_que.put( ("SL2 VC2 ARQ", status2.vcs[2].arq_state +10.2) )
		plot_que.put( ("SL2 VC3 ARQ", status2.vcs[3].arq_state +10.3) )
		time.sleep(0.12)



def plain_generate_payloads(skylink1:SkyLink, skylink2:SkyLink, vc, rate10, rate20, length):
	plot_que = create_realplot(14,24)
	sent1 = set()
	sent2 = set()
	t = rtime()
	rate1 = rate10 * NS.TIME_RATE + 1e-6
	rate2 = rate20 * NS.TIME_RATE + 1e-6
	sleep1 = 1 / rate1
	sleep2 = 1 / rate2
	next1 = t + sleep1
	next2 = t + sleep2
	t_end = rtime() + length
	while rtime() < (t_end + 15):
		plot_que.put(("unreceived by 2", len(sent1)))
		plot_que.put(("unreceived by 1", len(sent2)))
		st = max(0, min(next1,next2) - rtime())
		rsleep(min(0.15, st))
		t = rtime()
		if t < t_end:
			if t > next1:
				next1 = t + sleep1
				pl = os.urandom(random.randint(16,110))
				r = skylink1.push_pl(vc, pl)
				if (r < 0) and (r != -54):
					raise AssertionError("Skylink1 push error:{}".format(r))
				if r >= 0:
					sent1.add(pl)
			if t > next2:
				next2 = t + sleep2
				pl = os.urandom(random.randint(16,110))
				r = skylink2.push_pl(vc, pl)
				if (r < 0) and (r != -54):
					raise AssertionError("Skylink2 push error:{}".format(r))
				if r >= 0:
					sent2.add(pl)
		while True:
			rcv = skylink1.read_pl(vc)
			if rcv is None:
				break
			if not rcv in sent2:
				raise AssertionError("Received unsent package")
			sent2.remove(rcv)
		while True:
			rcv = skylink2.read_pl(vc)
			if rcv is None:
				break
			if not rcv in sent1:
				raise AssertionError("Received unsent package")
			sent1.remove(rcv)
	print("===========================")
	print("Generation over!")
	print("===========================")




def generate_payloads(skylink1:SkyLink, skylink2:SkyLink, vc, rate10, rate20, length, plot_unreceived=False):
	def getsleeptime(rate):
		if rate == 0:
			return np.Inf
		return random.expovariate(rate)
	if plot_unreceived:
		plot_que = create_realplot(14,24)
	else:
		plot_que = Queue(1000)
	sent1 = set()
	sent2 = set()
	t = rtime()
	rate1 = rate10 * NS.TIME_RATE
	rate2 = rate20 * NS.TIME_RATE
	sleep1 = getsleeptime(rate1)
	sleep2 = getsleeptime(rate2)
	next1 = t + sleep1
	next2 = t + sleep2
	t_end = rtime() + length
	while rtime() < (t_end + 15):
		if plot_unreceived:
			plot_que.put(("unreceived by 2 (vc:{})".format(vc), len(sent1)))
			plot_que.put(("unreceived by 1 (vc:{})".format(vc), len(sent2)))

		st = max(0, min(next1,next2) - rtime())
		rsleep(min(0.25, st))
		t = rtime()
		if t < t_end:
			if t > next1:
				next1 = t + getsleeptime(rate1)
				pl = os.urandom(random.randint(6,110))
				r = skylink1.push_pl(vc, pl)
				if (r < 0) and (r != -54):
					raise AssertionError("Skylink1 push error:{}".format(r))
				if r >= 0:
					sent1.add(pl)
			if t > next2:
				next2 = t + getsleeptime(rate2)
				pl = os.urandom(random.randint(6,110))
				r = skylink2.push_pl(vc, pl)
				if (r < 0) and (r != -54):
					raise AssertionError("Skylink2 push error:{}".format(r))
				if r >= 0:
					sent2.add(pl)

		while True:
			rcv = skylink1.read_pl(vc)
			if rcv is None:
				break
			if not rcv in sent2:
				raise AssertionError("Received unsent package")
			sent2.remove(rcv)
		while True:
			rcv = skylink2.read_pl(vc)
			if rcv is None:
				break
			if not rcv in sent1:
				raise AssertionError("Received unsent package")
			sent1.remove(rcv)
	print("======================")
	print("Generation over!")
	print("======================")
#=== PLOTTING LOOPS ===============================================================================================






