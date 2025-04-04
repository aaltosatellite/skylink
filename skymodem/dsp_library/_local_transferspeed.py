import multiprocessing as mpr
from multiprocessing import Queue
import socket
import time
import os
import zmq
from mtools.tools_system import mpr_set






def rcv_loop_tcp(port, que):
	time.sleep(0.3)
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.connect(("localhost", port))
	t0_rcv 	= 0.0
	speed 	= 0.0
	n_data 	= 0
	rcvlen_arr 	= list()

	s.settimeout(1.0)
	while True:
		try:
			k = s.recv(1024*128)
			if n_data == 0:
				t0_rcv = time.perf_counter()
			else:
				speed = (n_data+len(k)) / (time.perf_counter() - t0_rcv)
			n_data += len(k)
			rcvlen_arr.append(len(k))
		except socket.timeout:
			if speed > 0:
				que.put((speed, rcvlen_arr))
				print("Speed: {} Mbyte/s  ({} Mb  in {} packets)".format(round(1e-6*speed,1),  round(n_data*1e-6,3),  len(rcvlen_arr)))
			t0_rcv 	= 0.0
			speed 	= 0.0
			n_data 	= 0
			rcvlen_arr = list()


def main_tcp():
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	port = 4000 + 1
	s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	s.bind(("0.0.0.0", port))
	s.listen(5)

	q = Queue(100)
	rcv_process = mpr.Process(target=rcv_loop_tcp, args=(port, q))
	rcv_process.start()
	print("Rcv loop started.")

	cs,addr = s.accept()
	print("Rcv loop connection accepted.")

	pl = os.urandom(1024*256)
	cs.send(pl)
	print("Sent {} bytes".format(len(pl)))
	x = q.get(timeout=3)
	time.sleep(2)

	pl = os.urandom(1024*512)
	cs.send(pl)
	print("Sent {} bytes".format(len(pl)))
	time.sleep(2)







def foobar():
	context = zmq.Context()
	vc_base = 4001
	pub_sock = context.socket(zmq.PUB)
	pub_sock.bind("tcp://*:{}".format( str(vc_base + 1) ))
	pub_sock.set(zmq.RCVTIMEO, 1000)


	sub_sock = context.socket(zmq.SUB)
	sub_sock.bind("tcp://*:{}".format( str(vc_base + 1) ))
	sub_sock.subscribe(b"")
	sub_sock.set(zmq.RCVTIMEO, 1000)

def rcvloop_zmq(port, que):
	time.sleep(0.3)
	context = zmq.Context()
	sub_sock = context.socket(zmq.SUB)
	sub_sock.bind("tcp://*:{}".format( str(port) ))
	sub_sock.subscribe(b"")
	sub_sock.set(zmq.RCVTIMEO, 1000)
	t0_rcv 	= 0.0
	speed 	= 0.0
	n_data 	= 0
	rcvlen_arr 	= list()
	while True:
		try:
			k = sub_sock.recv()
			if n_data == 0:
				t0_rcv = time.perf_counter()
			else:
				speed = (n_data+len(k)) / (time.perf_counter() - t0_rcv)
			n_data += len(k)
			rcvlen_arr.append(len(k))
		except socket.timeout:
			if speed > 0:
				que.put((speed, rcvlen_arr))
				print("Speed: {} Mbyte/s  ({} Mb  in {} packets)".format(round(1e-6*speed,1),  round(n_data*1e-6,3),  len(rcvlen_arr)))
			t0_rcv 	= 0.0
			speed 	= 0.0
			n_data 	= 0
			rcvlen_arr = list()


def main_zmq():
	context = zmq.Context()
	port = 4002
	pub_sock = context.socket(zmq.PUB)
	pub_sock.bind("tcp://*:{}".format( str(port) ))
	pub_sock.set(zmq.RCVTIMEO, 1000)

	q = Queue(100)
	rcv_process = mpr.Process(target=rcvloop_zmq, args=(port, q))
	rcv_process.start()
	print("Rcv loop started.")
	time.sleep(1.0)

	pl = os.urandom(1024*10)
	pub_sock.send(pl)
	print("Sent {} bytes".format(len(pl)))
	x = q.get(timeout=3)
	time.sleep(2)

	pl = os.urandom(1024*200)
	pub_sock.send(pl)
	print("Sent {} bytes".format(len(pl)))
	time.sleep(2)










main_tcp()

#main_zmq()























