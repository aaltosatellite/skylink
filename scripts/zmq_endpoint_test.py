import random
import time
import threading
from zmq_endpoint import SkyLink
import zmq
from zmq_endpoint_test_tools import transfer_loop, SkyConfig, generate_payloads, plain_generate_payloads
# noinspection PyUnresolvedReferences
from zmq_endpoint_test_tools import NS, plot_arq, plot_txrx, plot_remaining, plot_hmac, rsleep, rtime
rint = random.randint
rng = random.random

MOD_TIME_TICKS = 16777216


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
	skylink1 = SkyLink(1, NS.TIME_RATE, 9600, 0, 0, conf1, len(conf1), rint(0,90000000))
	skylink2 = SkyLink(2, NS.TIME_RATE, 9600, 0, 0, conf2, len(conf2), rint(0,90000000))
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
	skylink1 = SkyLink(1, NS.TIME_RATE, 9600, 0, 0, conf1, len(conf1), rint(0,90000000))
	skylink2 = SkyLink(2, NS.TIME_RATE, 9600, 0, 0, conf2, len(conf2), rint(0,90000000))
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




def full_throttle():
	NS.TIME_RATE = 0.6
	NS.DIST_LAG = 0.011
	NS.CORRUPT_RATE = 0.02
	NS.LOSS_RATE = 0.02
	NS.USE_GLOBALS  = True

	# == zmq sockets and transfer loop =============================================
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
	# == zmq sockets and transfer loop =============================================


	# == SkyLink ===================================================================
	co1 = SkyConfig()
	co2 = SkyConfig()
	conf1 = co1.dump()
	conf2 = co2.dump()
	skylink1 = SkyLink(1, NS.TIME_RATE, 9600, 0, 0, conf1, len(conf1), rint(0,90000000))
	skylink2 = SkyLink(2, NS.TIME_RATE, 9600, 0, 0, conf2, len(conf2), rint(0,90000000))
	print("Skylinks started.", flush=True)
	time.sleep(0.5)
	# == SkyLink ===================================================================


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

	time.sleep(8)
	print("Initiating ARQ. Should Succeed.")
	skylink1.init_arq(0)
	time.sleep(8)

	print("Some payloads")
	thr6 = threading.Thread(target=plain_generate_payloads, args=(skylink1, skylink2, 0, 30.0, 10.0, 40))
	thr6.start()





def many_channels():
	NS.TIME_RATE = 0.6
	NS.DIST_LAG = 0.011
	NS.CORRUPT_RATE = 0.03
	NS.LOSS_RATE = 0.03
	NS.USE_GLOBALS  = True

	# == zmq sockets and transfer loop =============================================
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
	# == zmq sockets and transfer loop =============================================


	# == SkyLink ===================================================================
	co1 = SkyConfig()
	co2 = SkyConfig()
	conf1 = co1.dump()
	conf2 = co2.dump()
	skylink1 = SkyLink(1, NS.TIME_RATE, 2*9600, 0, 0, conf1, len(conf1), rint(0,90000000))
	skylink2 = SkyLink(2, NS.TIME_RATE, 2*9600, 0, 0, conf2, len(conf2), rint(0,90000000))
	print("Skylinks started.", flush=True)
	time.sleep(0.5)
	# == SkyLink ===================================================================


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

	time.sleep(8)
	print("Initiating ARQs. Should Succeed.")
	skylink1.init_arq(0)
	skylink1.init_arq(1)
	time.sleep(8)


	print("Some payloads")
	thr6 = threading.Thread(target=generate_payloads, args=(skylink1, skylink2, 0, 2.0, 2.0, 90, True))
	thr6.start()
	time.sleep(1)

	print("Some payloads")
	thr6 = threading.Thread(target=generate_payloads, args=(skylink1, skylink2, 1, 1.5, 0.5, 90, True))
	thr6.start()
	time.sleep(1)

	print("Some payloads")
	thr6 = threading.Thread(target=generate_payloads, args=(skylink1, skylink2, 3, 1.0, 1.3, 90, True))
	thr6.start()
	time.sleep(1)

	time.sleep(100)



def mac_stops():
	NS.TIME_RATE = 0.6
	NS.DIST_LAG = 0.011
	NS.CORRUPT_RATE = 0.03
	NS.LOSS_RATE = 0.03
	NS.USE_GLOBALS  = True

	# == zmq sockets and transfer loop =============================================
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
	# == zmq sockets and transfer loop =============================================


	# == SkyLink ===================================================================
	co1 = SkyConfig()
	co2 = SkyConfig()
	conf1 = co1.dump()
	conf2 = co2.dump()
	skylink1 = SkyLink(1, NS.TIME_RATE, 2*9600, 0, 0, conf1, len(conf1), rint(0,90000000))
	skylink2 = SkyLink(2, NS.TIME_RATE, 2*9600, 0, 0, conf2, len(conf2), rint(0,90000000))
	print("Skylinks started.", flush=True)
	time.sleep(0.5)
	# == SkyLink ===================================================================


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

	time.sleep(8)
	print("Initiating MAC. Should Succeed.")
	skylink1.push_pl(0, b"ASD")
	skylink2.push_pl(0, b"ASD")
	time.sleep(1)
	skylink1.push_pl(0, b"ASD")
	skylink2.push_pl(0, b"ASD")

	time.sleep(10)
	NS.CORRUPT_RATE = 0.99
	NS.LOSS_RATE    = 0.99

	time.sleep(100)






def mac_idle_in_start():
	NS.TIME_RATE = 0.8
	NS.DIST_LAG = 0.011
	NS.CORRUPT_RATE = 0.99
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
	skylink1 = SkyLink(1, NS.TIME_RATE, 9600, 0, 0, conf1, len(conf1), MOD_TIME_TICKS - 300000 + 50*1000)
	skylink2 = SkyLink(2, NS.TIME_RATE, 9600, 0, 0, conf2, len(conf2), MOD_TIME_TICKS - 300000 + 52*1000)
	print("Skylinks started.", flush=True)
	time.sleep(0.5)

	# == plots =========================================================================================================
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
	# == plots =========================================================================================================

	time.sleep(20)

	print("Some payloads")
	thr6 = threading.Thread(target=generate_payloads, args=(skylink1, skylink2, 0, 2.4, 0.0, 60, True))
	thr6.start()
	time.sleep(20)





if __name__ == '__main__':
	#tst1()
	#failed_arq_init_and_successful()
	#full_throttle()
	#many_channels()
	#mac_stops()
	mac_idle_in_start()
