import random
import zmq
import time
import threading
from realtime_plot.drawer import create_realplot
rint = random.randint
rng = random.random


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


def new_subber(port, subto:list):
	ctx = zmq.Context()
	sock = ctx.socket(zmq.SUB)
	sock.connect("tcp://127.0.0.1:{}".format(port))
	time.sleep(0.2)
	for s in subto:
		sock.subscribe(s)
	time.sleep(0.2)
	return sock


def new_subber_bind(port, subto:list):
	ctx = zmq.Context()
	sock = ctx.socket(zmq.SUB)
	sock.bind("tcp://127.0.0.1:{}".format(port))
	time.sleep(0.2)
	for s in subto:
		sock.subscribe(s)
	time.sleep(0.2)
	return sock


def new_publisher(port):
	ctx = zmq.Context()
	sock = ctx.socket(zmq.PUB)
	sock.connect("tcp://127.0.0.1:{}".format(port))
	time.sleep(0.2)
	return sock


def new_publisher_bind(port):
	ctx = zmq.Context()
	sock = ctx.socket(zmq.PUB)
	sock.bind("tcp://127.0.0.1:{}".format(port))
	time.sleep(0.2)
	return sock


def bind_subber_print(port):
	Su = new_subber_bind(port, [b""])
	time.sleep(0.2)
	while True:
		print("Got:",Su.recv())


def publisher_spam(port):
	Pu = new_publisher(port)
	while True:
		Pu.send(b"Asd!")
		print("spammed")
		time.sleep(0.7)



class Bookkeeper:
	def __init__(self):
		self.sent = set()
		self.confirmed = set()
		self.lock = threading.RLock()

	def mark_sent(self, pl:bytes):
		with self.lock:
			self.sent.add(pl)

	def mark_received(self, pl:bytes):
		with self.lock:
			if pl in self.sent:
				self.sent.remove(pl)
				self.confirmed.add(pl)

	def report(self):
		with self.lock:
			S = "{} unconfirmed.  {} confirmed.".format(len(self.sent), len(self.confirmed))
			print(S)




if __name__ == '__main__':
	que = create_realplot(200,40)

	for i in range(1000):
		time.sleep(0.1)
		name = random.randint(4,5)
		v = random.random()
		que.put( (str(name),v) )



