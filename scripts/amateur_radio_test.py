import random
import time
import threading
from amateur_skypacket import compile_packet
#from zmq_endpoint_test_tools import NS, plot_arq, plot_txrx, plot_remaining, plot_hmac, rsleep, rtime
rint = random.randint
rng = random.random
import os



A = compile_packet(b"MARKUS", b"12345")
B = compile_packet(b"MARKUS", b"12345")
C = compile_packet(b"MARKUS", os.urandom(179))
print("A:",A)
print("B:",B)
print("C:",C)
print("C:",b"b"[:-1])


