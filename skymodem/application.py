"""
This file will contain the main application logic for the SkyModem application.
"""
import threading
from kuokka.radio_loop import RadioLoop, ReceiverSettings
from cython_skylink import SkyLinkLoop, SkyConfiguration
from cython_skylink import num_virtual_channels, arq_state_on, arq_state_off, arq_state_in_init
import zmq
import time
import json
from datetime import datetime as dtime
from queue import Queue, Empty

DEBUG_PRINT_ON = True

def DBGPRINT(*args, **kwargs):
	if DEBUG_PRINT_ON:
		print(*args, **kwargs)



def sub_socket_loop(sub_sock:zmq.Socket, sub_que:Queue, ID, parent_obj):
	sub_sock.set(zmq.RCVTIMEO, 250)
	sub_sock.subscribe(b"")
	while parent_obj.on:
		try:
			rcv_msg = sub_sock.recv()
			DBGPRINT("++zmq-sub-socket-{} received {} bytes.".format(ID, len(rcv_msg)), flush=True)
			sub_que.put_nowait((ID, rcv_msg))
		except zmq.Again:
			pass
		except:
			break


def pub_socket_loop(pub_sock_dict:dict, pub_que:Queue, parent_obj):
	for k in pub_sock_dict.keys():
		pub_sock_dict[k].set(zmq.SNDTIMEO, 500)
	while parent_obj.on:
		try:
			ID, msg = pub_que.get(timeout=0.250)
			pub_sock_dict[ID].send(msg)
		except Empty:
			pass
		except:
			break


def bind_vc_sockets(vc_base, num_channels):
	context = zmq.Context()
	pub_sockets = list()
	sub_sockets = list()
	for i_vc in range(num_channels):
		pub_sock = context.socket(zmq.PUB)
		pub_sock.bind("tcp://*:{}".format( str(vc_base + i_vc*10) ))
		pub_sock.set(zmq.RCVTIMEO, 1000)
		pub_sockets.append(pub_sock)

		sub_sock = context.socket(zmq.SUB)
		sub_sock.bind("tcp://*:{}".format( str(vc_base + i_vc*10 + 1) ))
		sub_sock.subscribe(b"")
		sub_sock.set(zmq.RCVTIMEO, 1000)
		sub_sockets.append(sub_sock)

	return pub_sockets, sub_sockets, context



class SkyModem:
	def __init__(self, receiver_settings:ReceiverSettings, skylink_config:SkyConfiguration, hmac_key_list, vc_port_base):
		self.on = True
		self.receiver_settings = receiver_settings
		self.skylink_config = skylink_config
		self.hmac_key_list = hmac_key_list
		self.radio_loop = RadioLoop(rx_settings=receiver_settings)
		self.skylink_loop = SkyLinkLoop(config=skylink_config, key_list=hmac_key_list,
										que_payloads_from_radio=self.radio_loop.que_radio_to_skylink,
										que_payloads_to_radio=self.radio_loop.que_skylink_to_radio)
		self.arq_check_thread 			= threading.Thread(target=None, args=tuple())
		self.sub_que_process_thread 	= threading.Thread(target=None, args=tuple())
		self.skylink_reception_thread 	= threading.Thread(target=None, args=tuple())
		pub_sockets, sub_sockets, context = bind_vc_sockets(vc_base=vc_port_base, num_channels=num_virtual_channels)
		self.pub_sockets = pub_sockets
		self.sub_sockets = sub_sockets
		self.zmq_ctx = context
		self.sub_threads = list()
		self.session_id_list = [arq_state_off,] * num_virtual_channels
		self.sub_que = Queue(128)
		self.action_lock = threading.RLock()


	def is_ok(self):
		if not self.on:
			return False
		if not self.arq_check_thread.is_alive():
			return False
		if not self.sub_que_process_thread.is_alive():
			return False
		if not self.skylink_reception_thread.is_alive():
			return False
		for thrd in self.sub_threads:
			if not thrd.is_alive():
				return False
		if not self.skylink_loop.is_alive():
			return False
		if not self.radio_loop.is_ok():
			return False
		return True


	def close(self):
		self.on = False
		try:
			self.radio_loop.close()
		except:
			pass
		try:
			self.skylink_loop.close()
		except:
			pass
		self.arq_check_thread.join(timeout=1.0)
		self.sub_que_process_thread.join(timeout=1.0)
		self.skylink_reception_thread.join(timeout=1.0)
		for thrd in self.sub_threads:
			thrd.join(timeout=1.0)


	def start(self):
		for i,sub_sock in enumerate(self.sub_sockets):
			ID = i
			thrd = threading.Thread(target=sub_socket_loop, args=(sub_sock, self.sub_que, ID, self), daemon=True)
			thrd.start()
			self.sub_threads.append(thrd)
		self.skylink_loop.start()
		self.radio_loop.usrp_start() # TODO choose usrp or Soapy
		self.arq_check_thread = threading.Thread(target=self._check_arq_loop, args=tuple(), daemon=True)
		self.arq_check_thread.start()
		self.sub_que_process_thread = threading.Thread(target=self._sub_que_loop, args=tuple(), daemon=True)
		self.sub_que_process_thread.start()
		self.skylink_reception_thread = threading.Thread(target=self._skylink_reception_loop, args=tuple(), daemon=True)
		self.skylink_reception_thread.start()


	def _check_arq_loop(self):
		while self.on:
			try:
				with self.action_lock:
					self._check_arq_states()
				time.sleep(0.2)
			except Exception as e:
				DBGPRINT("Exception in process loop: ", e)
				self.close()
				break

	def _check_arq_states(self):
		state_d_l = self.skylink_loop.sky_get_state()
		session_id_list = [state_d_l[ichannel]["session_identifier"] for ichannel in range(num_virtual_channels)]
		for ichannel in range(num_virtual_channels):
			if (session_id_list[ichannel] != self.session_id_list[ichannel]) and (session_id_list[ichannel] == arq_state_off):
				metadata_dict = dict()
				metadata_dict["rsp"] = "arq_timeout"
				metadata_dict["vc"] = ichannel
				metadata_dict["session_identifier"] = session_id_list[ichannel]
				response_dict = dict()
				response_dict["packet_type"] = "control"
				response_dict["vc"] = ichannel
				response_dict["timestamp"] = dtime.now().isoformat()
				response_dict["metadata"] = metadata_dict
				self.pub_sockets[ichannel].send(json.dumps(response_dict))
				self.session_id_list[ichannel] = session_id_list[ichannel]
			if (session_id_list[ichannel] != self.session_id_list[ichannel]) and (session_id_list[ichannel] == arq_state_on):
				metadata_dict = dict()
				metadata_dict["rsp"] = "arq_connected"
				metadata_dict["vc"] = ichannel
				metadata_dict["session_identifier"] = session_id_list[ichannel]
				response_dict = dict()
				response_dict["packet_type"] = "control"
				response_dict["vc"] = ichannel
				response_dict["timestamp"] = dtime.now().isoformat()
				response_dict["metadata"] = metadata_dict
				self.pub_sockets[ichannel].send(json.dumps(response_dict))
				self.session_id_list[ichannel] = session_id_list[ichannel]



	def _skylink_reception_loop(self):
		while self.on:
			try:
				ichannel, rdata = self.skylink_loop.que_received_messages.get(timeout=0.15)
				if not ichannel in range(num_virtual_channels):
					DBGPRINT("vc number in skylink reception out of bounds: {}".format(ichannel))
					continue
				with self.action_lock:
					frame_d = dict()
					frame_d["packet_type"] 	= "tm"
					frame_d["timestamp"] 	= dtime.now().isoformat()
					frame_d["vc"] 			= ichannel
					frame_d["data"] 		= "".join( [("00"+hex(x)[2:])[-2:] for x in rdata] )
					meta_d = dict()
					meta_d["vc"] 		= ichannel
					frame_d["metadata"] = meta_d
					self.pub_sockets[ichannel].send(json.dumps(frame_d))
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Error in skylink_reception_loop: ", e)
				self.close()
				break


	def _sub_que_loop(self):
		while self.on:
			try:
				ID, msg = self.sub_que.get(timeout=0.25)
				DBGPRINT("++ID-msg pulled from zmq-sub-queue-{}.".format(ID))
				with self.action_lock:
					self._process_sub_que_frame(ID, msg)
			except Empty:
				pass
			except Exception as e:
				DBGPRINT("Exception in process loop: ", e)
				self.close()
				break


	def _process_sub_que_frame(self, ID, msg):
		if not ID in range(num_virtual_channels):
			DBGPRINT("ID not in vc range: {}.".format(ID))
			return
		ichannel = ID
		frame_dict = json.loads( msg )
		DBGPRINT("++json load successful: {}".format(frame_dict) , flush=True)
		if not type(frame_dict) == dict():
			DBGPRINT("json was not a dict.")
			return

		if "data" in frame_dict:
			hex_string = frame_dict["data"]
			hex_pairs = [hex_string[i*2:i*2+2] for i in range(len(hex_string)//2)]
			ints = [int(x, 16) for x in hex_pairs]
			data = bytes(ints)
			send_ret = self.skylink_loop.send(ichannel=ichannel, data=data)
			if send_ret < 0:
				DBGPRINT("sky_vc_push_packet_to_send error: {}".format(send_ret))

		if "metadata" in frame_dict:
			response_dict = dict()
			control_dict = frame_dict["metadata"]
			if not "cmd" in control_dict:
				DBGPRINT("No 'cmd' field in control_dict.")
				return
			ctrl_command = control_dict["cmd"]
			if ctrl_command == "get_state":
				DBGPRINT("command unimplemented 1")			# TODO
				list_of_vc_state_dicts 	= self.skylink_loop.sky_get_state()
				response_dict["rsp"] 	= "state"
				response_dict["state"] 	= list_of_vc_state_dicts
			elif ctrl_command == "flush":
				self.skylink_loop.flush(ichannel=ichannel)
			elif ctrl_command == "get_stats":
				DBGPRINT("command unimplemented 2")			# TODO
			elif ctrl_command == "clear_stats":
				self.skylink_loop.sky_diag_clear()
			elif ctrl_command == "set_config":
				DBGPRINT("command unimplemented 3")			# TODO
			elif ctrl_command == "get_config":
				DBGPRINT("command unimplemented 4")			# TODO
			elif ctrl_command == "arq_connect":
				self.skylink_loop.arq_connect(ichannel=ichannel)
				response_dict["rsp"] = "arq_connecting"
				response_dict["session_identifier"] = self.skylink_loop.sky_get_state()[ichannel]["session_identifier"] #session_identifier
			elif ctrl_command == "arq_disconnect":
				self.skylink_loop.arq_disconnect(ichannel=ichannel)
			elif ctrl_command == "mac_reset":
				DBGPRINT("command unimplemented 5")			# TODO
			elif ctrl_command == "set_sequences":
				DBGPRINT("command unimplemented 6")			# TODO
			elif ctrl_command == "debug":
				DBGPRINT("command unimplemented 7")			# TODO
			else:
				DBGPRINT("Unknown control command: {}".format(ctrl_command))
				return

			rsp_frame_dict = dict()
			rsp_frame_dict["packet_type"] 	= "control"
			rsp_frame_dict["timestamp"] 	= dtime.now().isoformat()
			rsp_frame_dict["metadata"] 		= response_dict
			self.pub_sockets[ichannel].send(json.dumps(rsp_frame_dict))



















def zmq_socket_instrumentation():
	from types import SimpleNamespace
	pub_sockets, sub_sockets, context = bind_vc_sockets(vc_base=7100, num_channels=4)
	sub_que = Queue(210)
	NS = SimpleNamespace()
	NS.on = True
	sub_threads = list()
	for i,sub_sock in enumerate(sub_sockets):
		ID = i
		thrd = threading.Thread(target=sub_socket_loop, args=(sub_sock, sub_que, ID, NS), daemon=True)
		thrd.start()
		sub_threads.append(thrd)
	print("zmq socket listen loop rungging.")
	while True:
		try:
			rcv = sub_que.get(timeout=0.33)
			print("\trcv:",rcv)
			ichannel, json_data = rcv
			frame_dict = json.loads( json_data )
			print("\tjson load successful:",frame_dict , flush=True)

		except Empty:
			pass
		except Exception as e:
			print("Exception breaks the que-get loop: ", e)
			break
	print("Exit of zmq socket listen loop.")




def tst_1(vc_base):
	pub_sockets, sub_sockets, context = bind_vc_sockets(7100, num_channels=num_virtual_channels)
	time.sleep(0.1)
	print("Sockets created.")


	print("Creating peer")
	peer_pub_1 = context.socket(zmq.SUB)

	print("Connecting peer")
	peer_pub_1.connect("tcp://localhost:{}".format( str(vc_base + 0*10) ))
	peer_pub_1.subscribe(b"")
	peer_pub_1.set(zmq.RCVTIMEO, 1350)
	time.sleep(0.2)

	print("Pub-0 sending test.")
	pub_sockets[0].send(b"Foobar!")
	time.sleep(0.2)
	print("")

	print("Peer receiving 1.")
	t0 = time.perf_counter()
	rcv1 = peer_pub_1.recv()
	dt1 = round((time.perf_counter() - t0) * 1000)
	print("rcv1: ",rcv1)
	print("rcv1 passed in {} ms".format(dt1))
	print("")

	print("Peer receiving 2.")
	t0 = time.perf_counter()
	rcv2 = None
	try:
		rcv2 = peer_pub_1.recv()
	except zmq.Again:
		print("(Again exception handled)")
	dt2 = round((time.perf_counter() - t0) * 1000)
	print("rcv2: ",rcv2)
	print("rcv2 passed in {} ms".format(dt2))








zmq_socket_instrumentation()
#tst_1(7100)




















