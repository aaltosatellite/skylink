import threading
import threading as thrd
from queue import Queue
from c_skylink import SkyLink, SkyConfiguration, mod_time_ticks, num_virtual_channels, arq_state_off, arq_state_on, arq_state_in_init
import time
from datetime import datetime as dtime

EKEY_SKY_PAYLOAD 			= 100
EKEY_SKY_ARQ_CONNECTED 		= 101
EKEY_SKY_ARQ_DISCONNECTED 	= 102

DEBUG_PRINT_ON = True
def DBGPRINT(*args, **kwargs):
	ts = "[{}]".format( dtime.now().isoformat()[-15:] )
	ts += " "*(17-len(ts)) + "[SkyLink]  " + " "
	first, args = args[0], args[1:]
	if DEBUG_PRINT_ON:
		print(ts+str(first), *args, **kwargs)


class SkyLinkLoop(threading.Thread):
	def __init__(self, config:SkyConfiguration, key_list:list, que_payloads_in, que_payloads_out, radio_tx_sample_que:Queue, radio_ready_ev:threading.Event):
		super(SkyLinkLoop, self).__init__()
		self.daemon = True
		self.config = config
		self.name = bytes(config.identity).decode("utf8")
		self.key_list = key_list
		self.skylink = SkyLink(config)
		self.skylink.set_hmac_keys(key_list)
		self.on = True
		self.que_payloads_from_dsp = que_payloads_in
		self.que_payloads_to_dsp = que_payloads_out
		self.radio_tx_sample_que = radio_tx_sample_que
		self.radio_ready = radio_ready_ev
		self.que_received_messages = Queue(1000)
		self.session_id_list = [(0,arq_state_off),] * num_virtual_channels
		self.lock = thrd.RLock()

	def close(self):
		self.on = False
		self.join(timeout=1.0)

	def send(self, ichannel, data):
		with self.lock:
			return self.skylink.sky_vc_push_packet_to_send(ichannel, data)

	def arq_connect(self, ichannel):
		with self.lock:
			return self.skylink.sky_vc_arq_connect(ichannel)

	def arq_disconnect(self, ichannel):
		with self.lock:
			return self.skylink.sky_vc_arq_disconnect(ichannel)

	def flush(self, ichannel):
		with self.lock:
			return self.skylink.sky_vc_arq_disconnect(ichannel)

	def get_arq_state(self, ichannel):
		with self.lock:
			return self.skylink.sky_vc_get_arq_state(ichannel)

	def carrier_sensed(self):
		with self.lock:
			self.skylink.carrier_sensed()

	def sky_get_state(self):
		with self.lock:
			return self.skylink.sky_get_state()

	def sky_get_stats(self):
		with self.lock:
			return self.skylink.sky_get_stats()

	def sky_diag_clear(self):
		with self.lock:
			return self.skylink.sky_diag_clear()

	def run(self):
		sleeptime = 0.0
		while self.on:
			sleeptime += 0.1e-3
			with self.lock:
				t_tick_mono = (int(time.monotonic() * 1000) % mod_time_ticks)
				self.skylink.sky_tick(t_tick_mono)
				while not self.que_payloads_from_dsp.empty():
					code, data, ts_mono = self.que_payloads_from_dsp.get_nowait()
					if code == "cs":
						self.skylink.carrier_sensed()
					if code == "pl":
						pl = data
						#assert ts_mono < t_tick_mono
						sky_rx_ret = self.skylink.sky_rx(pl, ts_mono)
						color_code = "\033[92m"  # Green (default when no errors)
						if sky_rx_ret == -7:
							color_code = "\033[93m"  # Yellow
						elif sky_rx_ret < 0:
							color_code = "\033[91m"  # Red
						DBGPRINT(f"Was given a downlink frame of {len(pl)} bytes. sky_rx returned {color_code}{sky_rx_ret}\033[0m]")
						sleeptime = 0.0

				state_d_l = self.skylink.sky_get_state()
				sessid_state_list = [ (state_d_l[ichannel]["session_identifier"], state_d_l[ichannel]["state"]) for ichannel in range(num_virtual_channels)]
				for ichannel, (sessid, state) in enumerate(sessid_state_list):
					if (sessid,state) != self.session_id_list[ichannel]:
						self.session_id_list[ichannel] = (sessid,state)
						DBGPRINT("VC {} ARQ moved to state [{}].".format(ichannel, {arq_state_on:"ON", arq_state_in_init:"INIT", arq_state_off:"OFF"}[state]))
						if state == arq_state_in_init:
							continue
						ekey = {arq_state_on:EKEY_SKY_ARQ_CONNECTED, arq_state_off:EKEY_SKY_ARQ_DISCONNECTED}[state]
						self.que_received_messages.put_nowait( (ekey, ichannel, sessid) )
						sleeptime = 0.0

				while True:
					if not (self.que_payloads_to_dsp.empty() and self.radio_tx_sample_que.empty() and self.radio_ready.is_set()):  # We want to feed the radio only as fast as it transmits. Maybe [.full()] instead of [not .empty()] ?
						break
					tx_i, frame_bytes = self.skylink.sky_tx()
					if tx_i == 0:
						break
					DBGPRINT("Transmitting a frame of {} bytes uplink.".format(len(frame_bytes)))
					self.que_payloads_to_dsp.put_nowait((frame_bytes,time.monotonic()))
					sleeptime = 0.0

				for ichannel in range(num_virtual_channels):
					while True:
						ri, rb = self.skylink.sky_vc_read_next_received(ichannel)
						if ri < 0:
							break
						DBGPRINT("VC {} reception gave {} bytes.".format(ichannel, len(rb)))
						self.que_received_messages.put_nowait( (EKEY_SKY_PAYLOAD, ichannel, rb) )
						sleeptime = 0.0

			if sleeptime > 0:
				time.sleep( min(sleeptime, 5e-3) )


























