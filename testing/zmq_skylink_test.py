import os
import threading
import zmq
import time
import random
from queue import Queue
from zmq_test_tools import new_subber_bind, new_publisher_bind, Bookkeeper

rint = random.randint
rng = random.random



PEERS = {5:7,
		 7:5}

RELATIVE_TIME_SPEED = 0.2
TOTAL_LOSS_CHANCE 	= 0.00
CORRUPT_CHANCE    	= 0.09
PING_TIME_S	      	= 0.005



def xsleep(t):
	time.sleep(t/RELATIVE_TIME_SPEED)

def xperf_counter():
	return time.perf_counter() * RELATIVE_TIME_SPEED



def get_another(i:int):
	r = rint(0,255)
	while r == i:
		r = rint(0,255)
	return r



def corrupt(pl:bytes, chance_per_byte):
	pl2 = bytearray(pl)
	for i in range(len(pl2)):
		if rng() < chance_per_byte:
			pl2[i] = get_another(pl2[i])
	return bytes(pl2)






class EtherRcv:
	def __init__(self, port, etherque:Queue):
		self.lag = 0.020
		self.log = list()
		self.sock = new_subber_bind(port, [b""])
		self.on = True
		self.etherque = etherque

	def close(self):
		self.on = False

	def run(self):
		print("+B", threading.current_thread().getName())
		self.sock.set(zmq.RCVTIMEO, 20)
		while self.on:
			try:
				msg = self.sock.recv()
			except:
				continue
			ts = xperf_counter()
			src = int.from_bytes(msg[0:4], byteorder="little")
			if src in PEERS:
				tgt = PEERS[src]
				pl = msg[4:]
				self.log.append( (src, ts, len(pl)) )
				thr = threading.Thread(target=ether_process, args=(tgt, pl, ts, self.etherque))
				thr.start()
				#print("2 Ether RCV from: {}".format(src))

			else:
				#print("Ether RCV from: {} (no such peer)".format(src))
				pass
		print("-B", threading.current_thread().getName())


class EtherSend:
	def __init__(self, port, etherque:Queue):
		self.sock = new_publisher_bind(port)
		self.log = list()
		self.etherque = etherque
		self.on = True

	def close(self):
		self.on = False

	def run(self):
		print("+A", threading.current_thread().getName())
		while self.on:
			try:
				k = self.etherque.get(timeout=0.5)
			except:
				continue
			(tgt, pl, ts) = k
			self.log.append( (tgt, ts, len(pl)) )
			a = int(tgt).to_bytes(4, "little")
			b = a + pl
			self.sock.send(b)
			#print("3 Ether SEND to: {} : {}".format(tgt, len(b)))
		print("-A", threading.current_thread().getName())



def ether_process(tgt:int, pl:bytes, ts:float, sendque:Queue):
	if rng() < TOTAL_LOSS_CHANCE:
		return
	pl = corrupt(pl, CORRUPT_CHANCE)
	wait = (ts+PING_TIME_S) - xperf_counter()
	if wait > 0:
		xsleep(wait*0.999)
	sendque.put( (tgt, pl, ts) )





def pl_generator(queue:Queue, tgt:int, rate:float):
	print("+G", threading.current_thread().getName())
	self = threading.current_thread()
	self.is_on = True
	while self.is_on:
		sleep = random.expovariate(rate)
		xsleep(sleep)
		pl = os.urandom(rint(8,120))
		queue.put(  (tgt, pl)  )
	print("-G", threading.current_thread().getName())



class SkylinkManager:
	def __init__(self, write_port, read_port):
		self.book = Bookkeeper()
		self.write_sock = new_publisher_bind(write_port)
		self.read_sock = new_subber_bind(read_port, [b""])
		self.push_queue = Queue(1000)
		self.etherque   = Queue(1000)
		self.pushed_to_A = list()
		self.pushed_to_B = list()
		self.read_from_A = list()
		self.read_from_B = list()
		self.ID_A = 5
		self.ID_B = 7
		self.rate_A = 2.0
		self.rate_B = 2.0
		self.generator_A = threading.Thread(target=pl_generator, args=(self.push_queue, self.ID_A, self.rate_A))
		self.generator_B = threading.Thread(target=pl_generator, args=(self.push_queue, self.ID_B, self.rate_B))
		self.on = True

	def close(self):
		self.on = False
		self.generator_A.is_on = False
		self.generator_B.is_on = False

	def run(self):
		print("+X", threading.current_thread().getName())
		self.generator_A.start()
		self.generator_B.start()
		self.read_sock.set(zmq.RCVTIMEO, 5)
		while self.on:
			try:
				(tgt,pl) = self.push_queue.get_nowait()
				msg = int(tgt).to_bytes(4, "little")
				msg = msg + b"\x00" + pl
				self.write_sock.send(msg)
				self.book.mark_sent(pl)
				#print("send:", pl)
				#print("1 Manager push to: ",tgt)
			except:
				pass

			try:
				rcv = self.read_sock.recv()
				pl = rcv[5:]
				self.book.mark_received(pl)
				#src = int.from_bytes(rcv[0:4], "little")
				#print("\t4 Manager rcv from: ", src)
			except:
				pass
		print("-X", threading.current_thread().getName())



if __name__ == '__main__':
	etherque_ = Queue(1000)
	etherRcv = EtherRcv(4440, etherque_)
	etherSend = EtherSend(4441, etherque_)
	manager = SkylinkManager(4442, 4443)

	t1 = threading.Thread(target=etherRcv.run, args=tuple())
	t2 = threading.Thread(target=etherSend.run, args=tuple())
	t3 = threading.Thread(target=manager.run, args=tuple())

	t1.start()
	t2.start()
	t3.start()

	for _ in range(160):
		time.sleep(3)
		manager.book.report()
		print("({} threads.)".format(threading.active_count()))

	etherRcv.close()
	etherSend.close()
	manager.close()

	for _ in range(3):
		time.sleep(0.6)
		print("{} threads.".format(threading.active_count()))















