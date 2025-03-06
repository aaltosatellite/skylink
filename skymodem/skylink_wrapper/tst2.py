import time
from cython_skylink import c_skylink
from cython_skylink.c_skylink import SkyConfiguration, SkyLink
from cython_skylink.skylink_process import SkyLinkLoop
from _tstools import RadioWay

key1_1 = (b"01-" + b"1"*40)[0:32]
key2_1 = (b"02-" + b"1"*40)[0:32]
key3_1 = (b"03-" + b"1"*40)[0:32]
key4_1 = (b"04-" + b"1"*40)[0:32]
key1_2 = (b"01-" + b"2"*40)[0:32]
key2_2 = (b"02-" + b"2"*40)[0:32]
key3_2 = (b"03-" + b"2"*40)[0:32]
key4_2 = (b"04-" + b"2"*40)[0:32]
keyset_1 = [key1_1,key2_1,key3_1,key4_1]
keyset_2 = [key1_2,key2_2,key3_2,key4_2]



print("time mod: ", c_skylink.mod_time_ticks / (1000 * 60), " min")


def tst_arq():
	conf1 = SkyConfiguration()
	conf2 = SkyConfiguration()
	conf1.vc[0].require_authentication = c_skylink.auth_flag_use_crc32 | c_skylink.auth_flag_auth_tx | c_skylink.auth_flag_require_seq #| c_skylink.auth_flag_require_auth
	conf1.vc[1].require_authentication = c_skylink.auth_flag_use_crc32 | c_skylink.auth_flag_auth_tx
	conf2.vc[0].require_authentication = c_skylink.auth_flag_use_crc32 | c_skylink.auth_flag_auth_tx | c_skylink.auth_flag_require_seq #| c_skylink.auth_flag_require_auth
	conf2.vc[1].require_authentication = c_skylink.auth_flag_use_crc32 | c_skylink.auth_flag_auth_tx


	conf1.identity = b"1234567"
	conf2.identity = b"9876543"

	skylink1 = SkyLink(configuration=conf1)
	skylink2 = SkyLink(configuration=conf2)

	skylink1.set_hmac_keys( keyset_1 )
	skylink2.set_hmac_keys( keyset_1 )

	print(" KEYS 1 ============================================")
	for i in range(4):
		print("   ", skylink1.get_hmac_key(i))
	print(" KEYS 1 ============================================")
	print(" KEYS 2 ============================================")
	for i in range(4):
		print("   ", skylink2.get_hmac_key(i))
	print(" KEYS 2 ============================================")



	tx = skylink1.sky_tx()
	print("tx: ",tx)


	print("(1)")
	print("1 Arq state: ", [skylink1.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("2 Arq state: ", [skylink2.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("1 to_tx: ", [skylink1.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	print("2 to_tx: ", [skylink2.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	tx1 = skylink1.sky_tx()
	tx2 = skylink2.sky_tx()
	print("1 tx: ",tx1)
	print("2 tx: ",tx2)
	rx1 = rx2 = None
	if tx1[0]:
		rx1 = skylink2.sky_rx(raw_frame_bytes=tx1[1])
	if tx2[0]:
		rx2 = skylink1.sky_rx(raw_frame_bytes=tx2[1])
	print("rx1, rx2: ", rx1, rx2)
	print("")

	skylink1.sky_vc_arq_connect(0)

	print("(2)")
	print("1 Arq state: ", [skylink1.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("2 Arq state: ", [skylink2.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("1 to_tx: ", [skylink1.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	print("2 to_tx: ", [skylink2.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	tx1 = skylink1.sky_tx()
	tx2 = skylink2.sky_tx()
	print("1 tx: ",tx1)
	print("2 tx: ",tx2)
	rx1 = rx2 = None
	if tx1[0]:
		rx1 = skylink2.sky_rx(raw_frame_bytes=tx1[1])
	if tx2[0]:
		rx2 = skylink1.sky_rx(raw_frame_bytes=tx2[1])
	print("rx1, rx2: ", rx1, rx2)
	print("")

	T2 = skylink2.get_tick_time()
	while not skylink2.can_send():
		T2 += 10
		skylink2.sky_tick(T2)
		print("     (tick) ", T2)

	print("(3)")
	print("1 Arq state: ", [skylink1.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("2 Arq state: ", [skylink2.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("1 to_tx: ", [skylink1.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	print("2 to_tx: ", [skylink2.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	tx1 = skylink1.sky_tx()
	tx2 = skylink2.sky_tx()
	print("1 tx: ",tx1)
	print("2 tx: ",tx2)
	rx1 = rx2 = None
	if tx1[0]:
		rx1 = skylink2.sky_rx(raw_frame_bytes=tx1[1])
	if tx2[0]:
		rx2 = skylink1.sky_rx(raw_frame_bytes=tx2[1])
	print("rx1, rx2: ", rx1, rx2)
	print("")

	print("(4)")
	print("1 Arq state: ", [skylink1.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("2 Arq state: ", [skylink2.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("1 to_tx: ", [skylink1.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	print("2 to_tx: ", [skylink2.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	tx1 = skylink1.sky_tx()
	tx2 = skylink2.sky_tx()
	print("1 tx: ",tx1)
	print("2 tx: ",tx2)
	rx1 = rx2 = None
	if tx1[0]:
		rx1 = skylink2.sky_rx(raw_frame_bytes=tx1[1])
	if tx2[0]:
		rx2 = skylink1.sky_rx(raw_frame_bytes=tx2[1])
	print("rx1, rx2: ", rx1, rx2)
	print("")

	print("(5)")
	print("1 Arq state: ", [skylink1.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("2 Arq state: ", [skylink2.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("1 to_tx: ", [skylink1.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	print("2 to_tx: ", [skylink2.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	tx1 = skylink1.sky_tx()
	tx2 = skylink2.sky_tx()
	print("1 tx: ",tx1)
	print("2 tx: ",tx2)
	rx1 = rx2 = None
	if tx1[0]:
		rx1 = skylink2.sky_rx(raw_frame_bytes=tx1[1])
	if tx2[0]:
		rx2 = skylink1.sky_rx(raw_frame_bytes=tx2[1])
	print("rx1, rx2: ", rx1, rx2)
	print("")

	print("(6)")
	print("1 Arq state: ", [skylink1.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("2 Arq state: ", [skylink2.sky_vc_get_arq_state(i) for i in (0, 1, 2, 3)])
	print("1 to_tx: ", [skylink1.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	print("2 to_tx: ", [skylink2.sky_vc_count_packets_to_tx(i, 1) for i in (0, 1, 2, 3)])
	tx1 = skylink1.sky_tx()
	tx2 = skylink2.sky_tx()
	print("1 tx: ",tx1)
	print("2 tx: ",tx2)
	rx1 = rx2 = None
	if tx1[0]:
		rx1 = skylink2.sky_rx(raw_frame_bytes=tx1[1])
	if tx2[0]:
		rx2 = skylink1.sky_rx(raw_frame_bytes=tx2[1])
	print("rx1, rx2: ", rx1, rx2)
	print("")






def tst_exchange():
	conf1 = SkyConfiguration()
	conf2 = SkyConfiguration()
	conf1.vc[0].require_authentication = c_skylink.auth_flag_use_crc32 | c_skylink.auth_flag_auth_tx | c_skylink.auth_flag_require_seq #| c_skylink.auth_flag_require_auth
	conf1.vc[1].require_authentication = c_skylink.auth_flag_use_crc32 | c_skylink.auth_flag_auth_tx
	conf2.vc[0].require_authentication = c_skylink.auth_flag_use_crc32 | c_skylink.auth_flag_auth_tx | c_skylink.auth_flag_require_seq #| c_skylink.auth_flag_require_auth
	conf2.vc[1].require_authentication = c_skylink.auth_flag_use_crc32 | c_skylink.auth_flag_auth_tx


	conf1.identity = b"Peer-01"
	conf2.identity = b"Peer-02"

	#skylink1 = SkyLink(conf_override=conf1)
	#skylink2 = SkyLink(conf_override=conf2)


	radioway = RadioWay()
	radioway.start()


	mdm1 = SkyLinkLoop(conf1, keyset_1, que_payloads_from_radio=radioway.pls_radio_to_skylink1, que_payloads_to_radio=radioway.pls_skylink1_to_radio)
	mdm2 = SkyLinkLoop(conf2, keyset_1, que_payloads_from_radio=radioway.pls_radio_to_skylink2, que_payloads_to_radio=radioway.pls_skylink2_to_radio)
	mdm1.start()
	mdm2.start()

	#print("skylink loops on...")
	#time.sleep(20)
	#print("go!")
	time.sleep(1)

	print("      (sending ABCD)")
	mdm1.send(0, b"ABCD")
	time.sleep(1)

	print("      (sending ABCD)")
	mdm1.send(0, b"MSG- 1>2 @ 0")
	mdm2.send(1, b"MSG- 2>1 @ 1")
	time.sleep(1)

	print("Peer-01 arq states:", [mdm1.get_arq_state(i) for i in (0,1,2,3)] )
	print("Peer-02 arq states:", [mdm2.get_arq_state(i) for i in (0,1,2,3)] )
	print("      (setting arq on)")
	mdm1.arq_connect(1)
	time.sleep(1)

	print("Peer-01 arq states:", [mdm1.get_arq_state(i) for i in (0,1,2,3)] )
	print("Peer-02 arq states:", [mdm2.get_arq_state(i) for i in (0,1,2,3)] )
	time.sleep(1)

	[print(i) for i in mdm1.sky_get_state()]
	[print(i) for i in mdm2.sky_get_state()]
	print("")
	print("")
	[print(i) for i in mdm1.sky_get_stats().items()]
	[print(i) for i in mdm2.sky_get_stats().items()]
	mdm2.sky_diag_clear()
	[print(i) for i in mdm2.sky_get_stats().items()]

#tst_arq()
tst_exchange()










