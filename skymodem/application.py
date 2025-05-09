"""
This file will contain the main application logic for the SkyModem application.
"""
import threading
from radio_loop import RadioLoop, RadioConfig
from dsp_library.kuokka.dsp_loop import DSPLoop
from dsp_library.kuokka.lib_receiver import DSPConfig
from dsp_library.kuokka.lib_tools import get_doppler_low_high
from dsp_library.kuokka.lib_tools import fractional_resampler_f_max_undisturbed
from skylink_wrapper.cython_skylink import SkyLinkLoop, SkyConfiguration, EKEY_SKY_ARQ_DISCONNECTED, EKEY_SKY_PAYLOAD, EKEY_SKY_ARQ_CONNECTED
from skylink_wrapper.cython_skylink import num_virtual_channels, arq_state_off
import zmq
import time
import json
from copy import deepcopy
from datetime import datetime as dtime
from queue import Queue, Empty
import os, struct
DEBUG_PRINT_ON = True

def DBGPRINT(*args, **kwargs):
	ts = "[{}]".format( dtime.now().isoformat()[-15:] )
	ts += " "*(17-len(ts)) + "[SkyModem] " + " "
	first, args = args[0], args[1:]
	if DEBUG_PRINT_ON:
		print(ts+str(first), *args, **kwargs)



def fetch_hmac_key(fpath):
	assert os.path.isfile(fpath)
	f = open(fpath, "rb")
	rd = f.read()
	f.close()
	key = json.loads(rd)
	assert type(key) in (tuple, list)
	for i in key:
		assert type(i) == int
		assert 0 <= i <= 255
	assert len(key) == 32
	return struct.pack("32b", *key), key




def sub_socket_loop(sub_sock:zmq.Socket, sub_que:Queue, ichannel, parent_obj):
	sub_sock.set(zmq.RCVTIMEO, 250)
	sub_sock.subscribe(b"")
	while parent_obj.on:
		try:
			rcv_msg = sub_sock.recv()
			sub_que.put_nowait((ichannel, rcv_msg))
		except zmq.Again:
			pass
		except Exception as e:
			DBGPRINT("Exception in zmq-subscribed socket loop: "+str(e))
			break


def bind_vc_sockets(vc_port_base, num_channels):
	context = zmq.Context()
	pub_sockets = list()
	sub_sockets = list()
	for i_vc in range(num_channels):
		pub_sock = context.socket(zmq.PUB)
		pub_sock.bind("tcp://*:{}".format( str(vc_port_base + i_vc*10) ))
		pub_sock.set(zmq.RCVTIMEO, 1000)
		pub_sockets.append(pub_sock)

		sub_sock = context.socket(zmq.SUB)
		sub_sock.bind("tcp://*:{}".format( str(vc_port_base + i_vc*10 + 1) ))
		sub_sock.subscribe(b"")
		sub_sock.set(zmq.RCVTIMEO, 1000)
		sub_sockets.append(sub_sock)

	return pub_sockets, sub_sockets, context





class SkyModem:
	def __init__(self, dsp_config:DSPConfig, radio_config:RadioConfig, skylink_config:SkyConfiguration, hmac_key_list, vc_port_base):
		self.on = True
		self.hmac_key_list = hmac_key_list
		self.que_samples_radio_to_dsp = Queue(500)
		self.que_payloads_dsp_to_sky = Queue(100)
		self.que_payloads_sky_to_dsp = Queue(1)
		self.que_samples_dsp_to_radio = Queue(1)

		self.radio_loop 	= RadioLoop(radio_config=radio_config, que_tx_samples_in=self.que_samples_dsp_to_radio, que_rx_samples_out=self.que_samples_radio_to_dsp)
		self.dsp_loop 		= DSPLoop(dsp_config=dsp_config, que_rx_samples_in=self.que_samples_radio_to_dsp, que_rx_payloads_out=self.que_payloads_dsp_to_sky,
								que_tx_payloads_in=self.que_payloads_sky_to_dsp, que_tx_samples_out=self.que_samples_dsp_to_radio)
		self.skylink_loop 	= SkyLinkLoop(config=skylink_config, key_list=hmac_key_list, que_payloads_in=self.que_payloads_dsp_to_sky, que_payloads_out=self.que_payloads_sky_to_dsp)
		self.sub_que_process_thread 	= threading.Thread(target=None, args=tuple())
		self.skylink_reception_thread 	= threading.Thread(target=None, args=tuple())
		pub_sockets, sub_sockets, context = bind_vc_sockets(vc_port_base=vc_port_base, num_channels=num_virtual_channels)
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
		if not self.dsp_loop.is_ok():
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
		try:
			self.dsp_loop.close()
		except:
			pass
		for thrd in (list(self.sub_threads) + [self.sub_que_process_thread, self.skylink_reception_thread]):
			try:
				thrd.join(timeout=1.0)
			except:
				pass


	def start(self):
		for i,sub_sock in enumerate(self.sub_sockets):
			ichannel = i
			thrd = threading.Thread(target=sub_socket_loop, args=(sub_sock, self.sub_que, ichannel, self), daemon=True)
			thrd.start()
			self.sub_threads.append(thrd)
		self.skylink_loop.start()
		self.dsp_loop.start()
		self.radio_loop.start()
		self.sub_que_process_thread 	= threading.Thread(target=self._zmq_to_modem_loop, args=tuple(), daemon=True)
		self.sub_que_process_thread.start()
		self.skylink_reception_thread 	= threading.Thread(target=self._modem_to_zmq_loop, args=tuple(), daemon=True)
		self.skylink_reception_thread.start()


	## PRIVATE FUNCTIONS =====================================================================================================================================================================
	## =======================================================================================================================================================================================
	def _modem_to_zmq_loop(self): # downlink
		while self.on:
			try:
				ekey, ichannel, rdata = self.skylink_loop.que_received_messages.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Exception (skylink_reception_loop): "+str(e))
				self.close()
				break
			assert ichannel in range(num_virtual_channels)
			with self.action_lock:
				self._process_skylink_msg(ekey=ekey, ichannel=ichannel, data=rdata)


	def _process_skylink_msg(self, ekey, ichannel, data):
		if ekey in (EKEY_SKY_ARQ_CONNECTED, EKEY_SKY_ARQ_DISCONNECTED):
			session_id = data
			assert type(session_id) == int
			metadata_dict = dict()
			if ekey == EKEY_SKY_ARQ_CONNECTED:
				metadata_dict["rsp"] = "arq_connected"
			else:
				metadata_dict["rsp"] = "arq_timeout"
			metadata_dict["vc"] = ichannel
			metadata_dict["session_identifier"] = session_id
			response_dict = dict()
			response_dict["packet_type"] = "control"
			response_dict["vc"] = ichannel
			response_dict["timestamp"] = dtime.now().isoformat()
			response_dict["metadata"] = metadata_dict
			self.pub_sockets[ichannel].send(json.dumps(response_dict).encode("utf8"))
			self.session_id_list[ichannel] = session_id
		if ekey == EKEY_SKY_PAYLOAD:
			assert type(data) == bytes
			DBGPRINT("[VC: {} -> pub-zmq. len: {}]: \n\033[96m{}\033[0m\n".format(ichannel, len(data), data))
			frame_d = dict()
			frame_d["packet_type"] 	= "tm"
			frame_d["timestamp"] 	= dtime.now().isoformat()
			frame_d["vc"] 			= ichannel
			frame_d["data"] 		= "".join( [("00"+hex(x)[2:])[-2:] for x in data] )
			meta_d = dict()
			meta_d["vc"] 		= ichannel
			frame_d["metadata"] = meta_d
			self.pub_sockets[ichannel].send(json.dumps(frame_d).encode("utf8"))


	def _zmq_to_modem_loop(self): # uplink
		while self.on:
			try:
				ichannel, uplink_json = self.sub_que.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Exception (sub_que_loop): "+str(e))
				self.close()
				break
			DBGPRINT("sub-zmq -> skylink-vc-{}. len: {}".format(ichannel, len(uplink_json)))
			with self.action_lock:
				self._process_uplink_json(ichannel, uplink_json)


	def _process_uplink_json(self, ichannel, uplink_json):
		if not ichannel in range(num_virtual_channels):
			DBGPRINT("error: ID not in vc range: {}".format(ichannel))
			return
		frame_dict = json.loads( uplink_json )
		if not type(frame_dict) == dict:
			DBGPRINT("error: json was not a dict:{}".format(type(frame_dict)))
			return

		if "data" in frame_dict:
			hex_string = frame_dict["data"]
			hex_pairs = [hex_string[i*2:i*2+2] for i in range(len(hex_string)//2)]
			ints = [int(x, 16) for x in hex_pairs]
			data = bytes(ints)
			send_ret = self.skylink_loop.send(ichannel=ichannel, data=data)
			if send_ret < 0:
				DBGPRINT("error: sky_vc_push_packet_to_send error: {}".format(send_ret))

		if "metadata" in frame_dict:
			response_dict = dict()
			control_dict = frame_dict["metadata"]
			if not "cmd" in control_dict:
				DBGPRINT("error: No 'cmd' field in control_dict")
				return
			ctrl_command = control_dict["cmd"]
			if ctrl_command == "get_state":
				list_of_vc_state_dicts 	= self.skylink_loop.sky_get_state()
				response_dict["rsp"] 	= "state"
				response_dict["state"] 	= list_of_vc_state_dicts
			elif ctrl_command == "flush":
				self.skylink_loop.flush(ichannel=ichannel)
				response_dict["rsp"] = "ack"
			elif ctrl_command == "get_stats":
				sky_stats_d = self.skylink_loop.sky_get_stats()
				response_dict["rsp"] = "stats"
				response_dict["skylink"] = sky_stats_d
				#response_dict["kuokka"] = dict()
			elif ctrl_command == "clear_stats":
				self.skylink_loop.sky_diag_clear()
			elif ctrl_command == "set_config":
				DBGPRINT("command unimplemented 1")			# TODO
				return
			elif ctrl_command == "get_config":
				DBGPRINT("command unimplemented 2")			# TODO
				return
			elif ctrl_command == "arq_connect":
				self.skylink_loop.arq_connect(ichannel=ichannel)
				response_dict["rsp"] = "arq_connecting"
				response_dict["session_identifier"] = self.skylink_loop.sky_get_state()[ichannel]["session_identifier"] #session_identifier
			elif ctrl_command == "arq_disconnect":
				self.skylink_loop.arq_disconnect(ichannel=ichannel)
				response_dict["rsp"] = "ack"
			elif ctrl_command == "mac_reset":
				DBGPRINT("command unimplemented 3")			# TODO
				return
			elif ctrl_command == "set_sequences":
				DBGPRINT("command unimplemented 4")			# TODO
				return
			elif ctrl_command == "debug":
				DBGPRINT("command unimplemented 5")			# TODO
				return
			elif ctrl_command == "set_baudrate":
				assert control_dict["baudrate"] in (9600, 9600*2, 9600*4), "invalid baudrate field in control_dict"
				self.dsp_loop.set_baudrate(control_dict["baudrate"])
				response_dict["rsp"] = "ack"
			elif ctrl_command == "set_skylink_config":
				r = self._set_skylink_config(control_dict["conf_idx"], control_dict["conf_value"])
				if r == 0:
					response_dict["rsp"] = "ack"
			elif ctrl_command == "reset_skylink_config":
				r = self._reset_skylink_config()
				if r == 0:
					response_dict["rsp"] = "ack"
			elif ctrl_command == "get_skylink_config":
				response_dict["skylink_config"] = self._get_skylink_config()
				response_dict["rsp"] = "skylink_config"
			else:
				DBGPRINT("Unknown control command: {}".format(ctrl_command))
				return

			rsp_frame_dict = dict()
			rsp_frame_dict["packet_type"] 	= "control"
			rsp_frame_dict["timestamp"] 	= dtime.now().isoformat()
			rsp_frame_dict["metadata"] 		= response_dict
			self.pub_sockets[ichannel].send(json.dumps(rsp_frame_dict).encode("utf8"))


	def _get_skylink_config(self):
		return self.skylink_loop.skylink.get_config_values()


	def _reset_skylink_config(self):
		new_config = SkyConfiguration(b"PyGS")
		new_skylink_loop  = SkyLinkLoop(config=new_config, key_list=self.hmac_key_list, que_payloads_in=self.que_payloads_dsp_to_sky, que_payloads_out=self.que_payloads_sky_to_dsp)
		self.skylink_loop.close()
		self.skylink_loop = new_skylink_loop
		self.skylink_loop.start()
		return 0


	def _set_skylink_config(self, conf_idx:int, conf_value):
		self.skylink_loop.skylink.set_config_value(idx=conf_idx, value=conf_value)
		return 0

def get_usrp_receiver_config(f_center, baudrate, max_signal_bw):
	from dsp_library.kuokka.lib_tools import determine_ftune_and_min_sr
	f_center_min, f_center_max = get_doppler_low_high(f_center=f_center, v_relative=7500.0*2)
	f_tune, minimum_samplerate = determine_ftune_and_min_sr(f_center_min=f_center_min, f_center_max=f_center_max, max_signal_bandwidth=max_signal_bw)
	print("Calculated minimum samplerate at {} ks/s".format( round(1.0e-3 * minimum_samplerate, 1) ))
	assert minimum_samplerate < 2e6
	if minimum_samplerate > 1e6:
		sr0 = 2e6
	else:
		sr0 = 1e6
	print("Using usrp radio config of: f_tune={} MHz,   sr0={} Ms/s".format( round(f_tune*1e-6, 3), round(sr0*1e-6, 3) ))
	radio_config 	= RadioConfig(mode="usrp", rx_sr=sr0, rx_f_tune=f_tune, rx_f_center=f_center, tx_sr=sr0, tx_f_tune=f_tune, tx_f_center=f_center)
	dsp_config 		= DSPConfig(rx_sr0=sr0, rx_f_tune=f_tune, rx_f_center=f_center, tx_sr0=sr0, tx_f_tune=f_tune, tx_f_center=f_center, baudrate=baudrate, bufferlen=800000, batch_maxlen=1024 * 16)
	return dsp_config, radio_config


def get_soapy_leecher_receiver_config(f_center, baudrate, f_tune, sr_hardware, max_signal_bw):
	f_center_min, f_center_max = get_doppler_low_high(f_center=f_center, v_relative=7500.0*2)
	f_center_min = f_center_min - max_signal_bw * 0.6
	f_center_max = f_center_max + max_signal_bw * 0.6
	plateu_minimum_halfwidth = max( abs(f_tune - f_center_min), abs(f_tune - f_center_max) )

	minimum_samplerate = int(2 * plateu_minimum_halfwidth)
	sr_leecher = max(1e6, 1e6*int(minimum_samplerate/1e6))
	while True:
		bw_leecher = 0.45 * (sr_leecher / sr_hardware)  	# This is the way SoapyShared computes the resampler filter length. (see SoapyLeecher.cpp:150)
		semilen_leecher = int(round(3.5 / bw_leecher))		# This is the way SoapyShared computes the resampler filter length. (see SoapyLeecher.cpp:150)
		f_max_undisturbed = fractional_resampler_f_max_undisturbed(sr0=sr_hardware, sr1=sr_leecher, halflen=semilen_leecher, f_cutoff_coeff=0.45)
		if f_max_undisturbed >= plateu_minimum_halfwidth:
			break
		sr_leecher += int(100e3)

	print("Calculated minimum samplerate at {} ks/s".format( round(1.0e-3 * minimum_samplerate, 1) ))
	print("Calculated necessary samplerate at {} ks/s".format( round(1.0e-3 * sr_leecher, 1) ))
	print("Using soapy-leecher radio config of: f_tune={} MHz,   sr0={} Ms/s".format( round(f_tune*1e-6, 3), round(sr_leecher*1e-6, 3) ))
	radio_config 	= RadioConfig(mode="soapy", rx_sr=sr_leecher, rx_f_tune=f_tune, rx_f_center=f_center, tx_sr=sr_leecher, tx_f_tune=f_tune, tx_f_center=f_center)
	dsp_config 		= DSPConfig(rx_sr0=sr_leecher, rx_f_tune=f_tune, rx_f_center=f_center, tx_sr0=sr_leecher, tx_f_tune=f_tune, tx_f_center=f_center, baudrate=baudrate, bufferlen=800000, batch_maxlen=1024 * 16)
	return dsp_config, radio_config




if __name__ == '__main__':
	key0 = b"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
	if key0 == b"":
		print("Check HMAC Key!")
		exit()
	hmac_keys = [key0, key0, key0, key0]
	skylink_config_ = SkyConfiguration(identity=b"PyGS")


	dsp_config_, radio_config_ = None, None
	import sys
	import argparse
	parser = argparse.ArgumentParser()
	parser.add_argument("--mode",    type=str, default="usrp", choices=("usrp", "soapy"), required=False)
	parser.add_argument("--vc_base", type=int, default=7100,   required=False)
	_args = parser.parse_args(sys.argv[1:])
	vc_base = _args.vc_base
	assert vc_base >= 1000
	assert vc_base < 60000

	if _args.mode == "soapy":
		dsp_config_, radio_config_ = get_soapy_leecher_receiver_config(f_center=437.1250e6 + 0e3, baudrate=9600, f_tune=436e6, sr_hardware=8e6, max_signal_bw=9600*4*1.2)
	else:
		assert _args.mode == "usrp"
		dsp_config_, radio_config_ = get_usrp_receiver_config(f_center=437.1250e6 + 0e3, baudrate=9600, max_signal_bw=9600*4*1.2)

	modem = SkyModem(dsp_config=dsp_config_, radio_config=radio_config_, skylink_config=skylink_config_, hmac_key_list=hmac_keys, vc_port_base=vc_base)
	modem.start()
	modem.dsp_loop.set_doppler_correction(False)
	try:
		while True:
			#print(threading.active_count(), "threads active")
			time.sleep(2.0)
			if not modem.is_ok():
				print("Modem is_ok() failed. Exiting.")
				break
	except KeyboardInterrupt:
		pass
	modem.close()
	sys.exit(0)
