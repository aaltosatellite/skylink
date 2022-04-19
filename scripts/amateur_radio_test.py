import random
import struct
import time
import threading
from zmq_endpoint import SkyLink
from amateur_skypacket import compile_packet
from zmq_endpoint_test_tools import transfer_loop, SkyConfig, generate_payloads, plain_generate_payloads
# noinspection PyUnresolvedReferences
from zmq_endpoint_test_tools import NS, plot_arq, plot_txrx, plot_remaining, plot_hmac, rsleep, rtime
rint = random.randint
rng = random.random
import os
import zmq




def tst1():
	NS.TIME_RATE = 0.8
	NS.DIST_LAG = 0.01
	NS.CORRUPT_RATE = 0.00
	NS.LOSS_RATE = 0.00
	NS.USE_GLOBALS  = True

	zmq_ctx = zmq.Context()
	tx_sock = zmq_ctx.socket(zmq.SUB)
	tx_sock.bind("tcp://127.0.0.1:3300")
	# noinspection PyUnresolvedReferences
	tx_sock.setsockopt(zmq.SUBSCRIBE, b"")
	rx_sock = zmq_ctx.socket(zmq.PUB)
	rx_sock.bind("tcp://127.0.0.1:3301")
	idd_map = {1:2, 2:1}
	tx_record = list()
	transfer_arguments = (tx_sock, rx_sock, idd_map, tx_record, NS.DIST_LAG, NS.CORRUPT_RATE, NS.LOSS_RATE)
	thr1 = threading.Thread(target=transfer_loop, args=transfer_arguments)
	thr1.start()
	time.sleep(1)


	co1 = SkyConfig()
	co2 = SkyConfig()
	conf1 = co1.dump()
	conf2 = co2.dump()
	skylink1 = SkyLink(1, NS.TIME_RATE, 9600, 0, 0, conf1, len(conf1))
	skylink2 = SkyLink(2, NS.TIME_RATE, 9600, 0, 0, conf2, len(conf2))
	print("Skylinks started.", flush=True)
	time.sleep(1)

	print("Sending packet")
	pl = compile_packet(b"MARKUS", b"FOOBAR!", 1)[3:]
	pl = struct.pack("i", 2) + pl
	for i in range(16):
		rx_sock.send( pl )
		time.sleep(0.15)

	print("Receiving...")
	print(skylink2.read_pl(3))

	time.sleep(60)









A = compile_packet(b"MARKUS", b"12345", 0)
B = compile_packet(b"MARKUS", b"12345", 1)
assert (len(A) == (len(B)-3))
assert (A[1:] == B[4:])
print(A)
print(B[3:])

C = compile_packet(b"MARKUS", os.urandom(179), 1)
print("A:",A)
print("B:",B)
print("C:",C)
print("C:",b"b"[:-1])


tst1()



