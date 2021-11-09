import os
import numpy as np
from matplotlib import pyplot as plt
import random
import time
rint = random.randint
from queue import Queue
import math
import threading
from collections import deque



LISTENING 			= 0		#a state flag with no semantic meaning
SENDING 			= 1		#a state flag with no semantic meaning
MSG_SEND_TIME_MS 	= 4		#time it takes to send one message.
SLOW_FACTOR		 	= 12	#time is slowed down by this factor. Makes simulating millisecond-scale stuff nicer.

MAX_WINDOW_SIZE 	= 160
MIN_WINDOW_SIZE 	= 30

PL_GEN_REFERENCE = deque()
PL_RCV_REFERENCE = deque()
PL_MISS_REFERENCE = deque()

def xsleep(t):
	time.sleep(t*SLOW_FACTOR)

def xperf_counter():
	return time.perf_counter()/SLOW_FACTOR




class MAC:
	def __init__(self, T, window, peer_window, gap, tail):
		self.window = window
		self.peer_window = peer_window
		self.gap = gap
		self.tail = tail
		self.T = self.wrap(T)

	def wrap(self, t):
		#return t
		return (t + self.cycle()) % self.cycle()

	def cycle(self):
		#return self.window + self.gap + self.peer_window + self.gap
		return self.window + self.gap + self.peer_window + self.tail

	def get_own_window_remaining(self, ms_now):
		dt = self.wrap(ms_now - self.T)
		#dt = dt % self.cycle()
		return self.window - dt

	def get_peer_window_remaining(self, ms_now):
		dt = self.wrap(ms_now - (self.T + self.window + self.gap))
		#dt = dt % self.cycle()
		return self.peer_window - dt

	def can_send(self, ms_now):
		return self.get_own_window_remaining(ms_now) > 0

	def cycle_epoch(self, ms_now):
		return (ms_now - self.T) % self.cycle()

	def update_belief(self, ms_now, length, remaining):
		if length != self.peer_window:
			self.peer_window = length

		#implied_epoch_start = self.wrap(ms_now + remaining + self.gap)
		implied_epoch_start = self.wrap(ms_now + remaining + self.tail)
		self.T = implied_epoch_start







class MsgGenerator(threading.Thread):
	def __init__(self, rate, que:Queue, parent):
		super(MsgGenerator, self).__init__()
		self.setDaemon(True)
		self.rate = rate
		self.parent = parent
		self.que = que
		self.on = True

	def close(self):
		self.on = False

	def run(self):
		while self.on:
			xsleep(random.expovariate(self.rate))
			pl = os.urandom(8)
			self.que.put_nowait(pl)
			PL_GEN_REFERENCE.append((pl, self.parent.own_name, xperf_counter()))
			print(">>GEN for:",self.parent.own_name, pl[0:2].hex())




class Side:
	def __init__(self, window, peer_window, gap, tail, time_shift, ether_que:Queue, own_name, pair_name):
		self.time_shift = time_shift
		self.t0 = self.time_ms()
		self.mac = MAC(self.t0, window, peer_window, gap, tail)
		self.sendque = Queue(1000)
		self.rcvque  = Queue(1000)
		self.ether_que  = ether_que
		self.state = LISTENING
		self.received_list = list()
		self.msgGenerator = MsgGenerator(9.0, self.sendque, self)
		self.own_name = own_name
		self.pair_name = pair_name
		self.on = True

	def close(self):
		self.msgGenerator.close()
		self.on = False

	def time_ms(self):
		return int((xperf_counter()+self.time_shift) * 1000)

	def ideal_send_rate(self):
		n = math.floor(self.mac.window / MSG_SEND_TIME_MS)
		f = n / self.mac.cycle()
		return f

	def send_procedure(self, now_ms):
		if self.sendque.empty():
			return
		if not self.mac.can_send(now_ms):
			return
		pl = self.sendque.get_nowait()
		msg = (self.mac.window, self.mac.get_own_window_remaining(now_ms), pl)
		self.ether_que.put_nowait((msg, self.pair_name, xperf_counter()))
		xsleep(MSG_SEND_TIME_MS/1000.0)


	def recv_procedure(self, now_ms):
		if self.rcvque.empty():
			return
		msg = self.rcvque.get_nowait()
		peer_window, peer_remaining, pl = msg
		self.mac.update_belief(now_ms, peer_window, peer_remaining)
		self.received_list.append((pl, self.own_name, xperf_counter()))
		PL_RCV_REFERENCE.append((pl, self.own_name, xperf_counter()))
		print("<<RCV to:",self.own_name, pl[0:2].hex())


	def resize_window_if_needed(self):
		t_send_ms = self.sendque.qsize() * MSG_SEND_TIME_MS
		if t_send_ms > self.mac.window:
			self.mac.window = min(self.mac.window+10, MAX_WINDOW_SIZE)
		if t_send_ms < self.mac.window/2:
			self.mac.window = max(self.mac.window-10, MIN_WINDOW_SIZE)
		if self.sendque.empty(): #this is important! All windows should have at least one message, even empty.
			self.sendque.put_nowait(b"")
			print(">0")


	def run(self):
		self.msgGenerator.start()
		while self.on:
			xsleep(0.0005)
			now_ms = self.time_ms()
			if self.mac.can_send(now_ms):
				if self.state != SENDING:
					self.resize_window_if_needed()
					print("\t{} sending".format(self.own_name))
				self.state = SENDING
				self.send_procedure(now_ms)
			else:
				self.state = LISTENING
			#Observe indentation: We process received messages at all times. We might not get them at all times.
			self.recv_procedure(now_ms)



class Ether(threading.Thread):
	def __init__(self, const_delay):
		super(Ether, self).__init__()
		self.ether_que = Queue(1000)
		self.ether_pool = list()
		self.on = True
		self.setDaemon(True)
		self.const_delay = const_delay
		self.sides = dict()

	def delay(self):
		return self.const_delay

	def close(self):
		self.on = False

	def run(self):
		while self.on:
			try:
				tx = self.ether_que.get(block=True, timeout=0.003)
				self.ether_pool.append(tx)
				self.ether_pool = sorted(self.ether_pool, key=lambda k: k[2])
			except:
				pass

			now = xperf_counter()
			while True:
				if not self.ether_pool:
					break
				if now > (self.ether_pool[0][2] + self.delay()):
					tx = self.ether_pool.pop(0)
					msg, tgt, sendtime = tx
					side = self.sides[tgt]
					if side.state == LISTENING:
						side.rcvque.put_nowait(msg)
					else:
						PL_MISS_REFERENCE.append((msg[2], tgt, xperf_counter()))
						print("><><MISS to:", tgt, msg[2].hex())
				else:
					break


class DataScrubber(threading.Thread):
	def __init__(self, sides):
		super(DataScrubber, self).__init__()
		self.sides = sides
		self.setDaemon(True)
		self.on = True
		self.states = dict()
		for side in self.sides:
			self.states[side.own_name] = list()

	def run(self) -> None:
		while self.on:
			xsleep(0.0005)
			for side in self.sides:
				statelist = self.states[side.own_name]
				state = (xperf_counter(), side.state, side.mac.T, side.mac.window)
				statelist.append(state)

	def close(self):
		self.on = False


Eth = Ether(0.0015)
sideA = Side(50,50,150,5,random.random()*1000000, ether_que=Eth.ether_que, own_name="A",pair_name="B")
sideB = Side(50,50,150,5,random.random()*1000000, ether_que=Eth.ether_que, own_name="B",pair_name="A")
Eth.sides["A"] = sideA
Eth.sides["B"] = sideB

print("starting....")
t1 = threading.Thread(target=sideA.run, args=tuple())
t2 = threading.Thread(target=sideB.run, args=tuple())
t1.start()
t2.start()
Eth.start()

scrubber = DataScrubber((sideA,sideB))
scrubber.start()

print("Started.")
time.sleep(20)

Eth.close()
sideA.close()
sideB.close()
scrubber.close()
time.sleep(2)

n_generated = len(PL_GEN_REFERENCE)
n_lost = len(PL_MISS_REFERENCE)
n_received = len(PL_RCV_REFERENCE)

print(100*round(n_received / n_generated, 2), "% received")
print(100*round(n_lost / n_generated, 2), "% lost")

dataA = np.array(scrubber.states["A"])
dataB = np.array(scrubber.states["B"])

t_A = dataA[:,0]
s_A = dataA[:,1]
T0_A = dataA[:,2]
w_A = dataA[:,3]

t_B = dataB[:,0]
s_B = dataB[:,1]
T0_B = dataB[:,2]
w_B = dataB[:,3]

#plt.subplot(211)
plt.plot(t_A, s_A*10)
plt.plot(t_B, s_B*10)
#plt.grid()

#plt.subplot(212)
plt.plot(t_A, T0_A)
plt.plot(t_B, T0_B)

#plt.plot(t_A, w_A)
#plt.plot(t_B, w_B)


plt.grid()
plt.show()





def TST1():
	def get_ms():
		return int(time.perf_counter()*1000)


	for i in range(1000):
		delta1 = rint(-300, 300)
		delta2 = rint(-300, 300)
		t0 = rint(1000,100000)
		mac1 = MAC(t0+delta1, 43, 61, 52, 6)
		mac2 = MAC(t0+delta2, 61, 43, 52, 6)

		now_ = get_ms()
		remaining2 = mac2.get_own_window_remaining(now_)
		while remaining2 < (mac2.window * 0.7):
			time.sleep(0.03 * random.random())
			now_ = get_ms()
			remaining2 = mac2.get_own_window_remaining(now_)

		mac1.update_belief(now_, mac2.window, mac2.get_own_window_remaining(now_))

		#print(mac1.get_peer_window_remaining(now), mac2.get_own_window_remaining(now))

		assert mac1.get_peer_window_remaining(now_) == mac2.get_own_window_remaining(now_)
		assert mac1.get_own_window_remaining(now_) == mac2.get_peer_window_remaining(now_)
		for _ in range(10):
			rt_ = random.randint(1, 1000000000000)
			assert mac1.get_peer_window_remaining(rt_) == mac2.get_own_window_remaining(rt_)
			assert mac1.get_own_window_remaining(rt_) == mac2.get_peer_window_remaining(rt_)


		if i % 100 == 0:
			print(i)










