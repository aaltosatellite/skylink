import random
import time
import threading
from zmq_endpoint import SkyLink
import zmq
from zmq_endpoint_test_tools import transfer_loop, SkyConfig, generate_payloads
# noinspection PyUnresolvedReferences
from zmq_endpoint_test_tools import NS, plot_arq, plot_txrx, plot_remaining, plot_hmac
rint = random.randint
rng = random.random



def tst1():
	zmq_ctx = zmq.Context()
	tx_sock = zmq_ctx.socket(zmq.SUB)
	tx_sock.bind("tcp://127.0.0.1:3300")
	# noinspection PyUnresolvedReferences
	tx_sock.setsockopt(zmq.SUBSCRIBE, b"")
	rx_sock = zmq_ctx.socket(zmq.PUB)
	rx_sock.bind("tcp://127.0.0.1:3301")
	idd_map = {1:2, 2:1}
	#txsock, rxsock, idd_map, lista, lag, corrupt_rate, loss_rate
	tx_record = list()

	NS.TIME_RATE = 0.6
	NS.DIST_LAG = 0.007
	NS.CORRUPT_RATE = 0.01
	NS.LOSS_RATE = 0.01
	NS.USE_GLOBALS  = True

	arguments = (tx_sock, rx_sock, idd_map, tx_record, NS.DIST_LAG, NS.CORRUPT_RATE, NS.LOSS_RATE)
	thr1 = threading.Thread(target=transfer_loop, args=arguments)
	thr1.start()
	time.sleep(0.4)

	co1 = SkyConfig()
	co2 = SkyConfig()
	conf1 = co1.dump()
	conf2 = co2.dump()
	skylink1 = SkyLink(1, NS.TIME_RATE, 9600, 0, 0, conf1, len(conf1))
	skylink2 = SkyLink(2, NS.TIME_RATE, 9600, 0, 0, conf2, len(conf2))
	time.sleep(0.4)

	thr2 = threading.Thread(target=plot_txrx, args=(skylink1,skylink2))
	thr2.start()
	time.sleep(0.2)

	thr3 = threading.Thread(target=plot_remaining, args=(skylink1, skylink2))
	thr3.start()
	time.sleep(0.2)

	#thr4 = threading.Thread(target=plot_hmac, args=(skylink1, skylink2))
	#thr4.start()
	#time.sleep(0.2)

	thr5 = threading.Thread(target=plot_arq, args=(skylink1, skylink2))
	thr5.start()
	time.sleep(0.2)

	time.sleep(5)
	skylink1.push_pl(1, b"PING!")
	skylink1.push_pl(1, b"PING!")
	print("PING!")

	time.sleep(20)
	skylink1.init_arq(0)
	time.sleep(4)

	thr6 = threading.Thread(target=generate_payloads, args=(skylink1, skylink2, 0, 2.4, 0.0, 60, True))
	thr6.start()

	time.sleep(0.2)

	for i in range(100000):
		time.sleep(4)
		NS.DIST_LAG = NS.DIST_LAG * 1.03
		NS.CORRUPT_RATE = NS.CORRUPT_RATE * 1.09
		NS.LOSS_RATE = NS.LOSS_RATE * 1.09
		print("Lag: {}ms".format( NS.DIST_LAG*1000))
		print("Corrupt:", NS.CORRUPT_RATE)
		print("Loss:", NS.LOSS_RATE)



def failed_arq_init_and_successful():
	NS.TIME_RATE = 0.5
	NS.DIST_LAG = 0.012
	NS.CORRUPT_RATE = 0.99
	NS.LOSS_RATE = 0.99
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
	time.sleep(0.5)

	# == plots =====================================================================
	thr2 = threading.Thread(target=plot_txrx, args=(skylink1,skylink2))
	thr2.start()
	time.sleep(0.2)

	thr3 = threading.Thread(target=plot_remaining, args=(skylink1, skylink2))
	thr3.start()
	time.sleep(0.2)

	#thr4 = threading.Thread(target=plot_hmac, args=(skylink1, skylink2))
	#thr4.start()
	#time.sleep(0.2)

	thr5 = threading.Thread(target=plot_arq, args=(skylink1, skylink2))
	thr5.start()
	time.sleep(0.2)
	# == plots =====================================================================

	time.sleep(10)

	print("Initiating ARQ. Should Fail.")
	skylink1.init_arq(0)
	time.sleep(30 / NS.TIME_RATE)

	NS.TIME_RATE = 0.5
	NS.DIST_LAG = 0.012
	NS.CORRUPT_RATE = 0.01
	NS.LOSS_RATE = 0.01
	NS.USE_GLOBALS  = True
	print("Initiating ARQ. Should Succeed.")
	skylink1.init_arq(0)
	time.sleep(5)

	print("Some payloads")
	thr6 = threading.Thread(target=generate_payloads, args=(skylink1, skylink2, 0, 2.4, 0.0, 60, True))
	thr6.start()
	time.sleep(20)







if __name__ == '__main__':
	tst1()
	#failed_arq_init_and_successful()

