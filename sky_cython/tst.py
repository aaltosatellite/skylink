from cython_skylink import c_skylink
from cython_skylink.c_skylink import SkyConfiguration, SkyLink




conf = SkyConfiguration()
conf2 = SkyConfiguration()
conf.identity = b"1234567"
conf2.identity = b"9876543"
print("B")

skylink = SkyLink(conf_override=conf)
skylink2 = SkyLink(conf_override=conf2)

key1 = (b"01-" + b"0"*40)[0:32]
key2 = (b"02-" + b"0"*40)[0:32]
key3 = (b"03-" + b"0"*40)[0:32]
key4 = (b"04-" + b"0"*40)[0:32]

skylink.set_hmac_keys( [key1,key2,key3,key4] )
print(" KEYS ============================================")
for i in range(4):
	print("   ", skylink.get_hmac_key(i))
print(" KEYS ============================================")

txlist = list()

print(skylink.sky_tick(300))
print(skylink.sky_tick(301))
print(skylink.sky_tick(301))
print("1 ---")
print(skylink.sky_tx())
print("1 ---")
for _ in range(21):
	print(skylink.sky_vc_push_packet_to_send(0, b"ABCDEFG"))
print(skylink.sky_vc_push_packet_to_send(1, b"AAAAAAAAAAAA"))
print(skylink.sky_vc_push_packet_to_send(2, b"AAAAAAAAAAAA"))
print(skylink.sky_vc_push_packet_to_send(3, b"AAAAAAAAAAAA"))
print("2 ---------")
print(skylink.sky_tx())
print(skylink.sky_tx())
print(skylink.sky_tx())
n_rcvd0 = 0
for i in range(30):
	print("buffer 0 full:  ", skylink.sky_vc_send_buffer_is_full(0))
	print("buffer 1 full:  ", skylink.sky_vc_send_buffer_is_full(1))
	print("que 0:          ", skylink.sky_vc_count_packets_to_tx(0, 1))
	print("que 1:          ", skylink.sky_vc_count_packets_to_tx(1, 1))
	print("que 2:          ", skylink.sky_vc_count_packets_to_tx(2, 1))
	print("que 3:          ", skylink.sky_vc_count_packets_to_tx(3, 1))
	print(skylink.sky_tick(1201 + i*9))
	print(skylink2.sky_tick(1201 + i*9))
	itx, txbts = skylink.sky_tx()
	print( (itx, txbts))
	if txbts:
		txlist.append(txbts)
		rx_ret = skylink2.sky_rx(raw_frame_bytes=txbts)
		print("(rx ret: ", rx_ret)
	print("    # SL2 rx buffer: ", [skylink2.sky_vc_count_readable_rcv_packets(ich) for ich in (0,1,2,3)])
	print("    # SL2 send que:  ", [skylink2.sky_vc_count_packets_to_tx(ich, 1) for ich in (0,1,2,3)])
	if skylink2.sky_vc_count_readable_rcv_packets(0) and 0:
		rcv_ret = skylink2.sky_vc_read_next_received(0)
		n_rcvd0 += 1
		print("        n_rcv_ret: ",n_rcvd0)
		print("        rcv ret 0:  ",rcv_ret)
	print("    # SL2 tx:        ", skylink2.sky_tx())
	print("")

print("3 ---")



#skylink.tx(b"AAAAAAAAA")
#skylink.tx(b"AAAAAAAAA")

#print(skylink.rx())
#print(skylink.rx())

#print("C")
