import threading
import threading as thrd
from queue import Queue
from c_skylink import SkyLink, SkyConfiguration, mod_time_ticks
import time




class SkyLinkLoop(threading.Thread):
	def __init__(self, config:SkyConfiguration, key_list:list, que_payloads_from_radio, que_payloads_to_radio):
		super(SkyLinkLoop, self).__init__()
		self.daemon = True
		self.config = config
		self.name = bytes(config.identity).decode("utf8")
		self.key_list = key_list
		self.skylink = SkyLink(config)
		self.skylink.set_hmac_keys(key_list=key_list)
		self.on = True
		self.que_payloads_from_radio = que_payloads_from_radio
		self.que_payloads_to_radio = que_payloads_to_radio
		self.que_received_messages = Queue(1000)
		self.lock = thrd.RLock()

	def close(self):
		self.on = False

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
				self.skylink.sky_tick( (int(time.time() * 1000) % mod_time_ticks) )
				while not self.que_payloads_from_radio.empty():
					self.skylink.sky_rx( self.que_payloads_from_radio.get_nowait() )
					sleeptime = 0.0

				while True:
					if not self.que_payloads_to_radio.empty():  # We want to feed the radio only as fast as it transmits. Maybe [.full()] instead of [not .empty()] ?
						break
					tx_i, frame_bytes = self.skylink.sky_tx()
					if tx_i == 0:
						break
					self.que_payloads_to_radio.put_nowait(frame_bytes)
					sleeptime = 0.0

				for ichannel in (0,1,2,3):
					while True:
						ri, rb = self.skylink.sky_vc_read_next_received(ichannel)
						if ri < 0:
							break
						self.que_received_messages.put_nowait( (ichannel, rb) )
						print("{} received at {}:  ".format(self.name, ichannel),ichannel, rb)
						sleeptime = 0.0

			if sleeptime > 0:
				time.sleep( min(sleeptime, 5e-3) )


























