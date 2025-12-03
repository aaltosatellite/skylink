from cython_skylink.skylink_process import SkyLinkLoop
import threading
import time
from queue import Queue


class RadioWay(threading.Thread):
    def __init__(self):
        super(RadioWay, self).__init__()
        self.daemon = True
        self.pls_radio_to_skylink1 = Queue(90)
        self.pls_radio_to_skylink2 = Queue(90)
        self.pls_skylink1_to_radio = Queue(3)
        self.pls_skylink2_to_radio = Queue(3)
        self.on = True

    def close(self):
        self.on = False

    def run(self):
        while self.on:
            while not self.pls_skylink1_to_radio.empty(): # skylink1.que_payloads_to_radio
                pl = self.pls_skylink1_to_radio.get_nowait()
                self.pls_radio_to_skylink2.put_nowait(pl)

            while not self.pls_skylink2_to_radio.empty():
                pl = self.pls_skylink2_to_radio.get_nowait()
                self.pls_radio_to_skylink1.put_nowait(pl)

            time.sleep(0.01)
