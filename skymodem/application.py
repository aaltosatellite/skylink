
"""
This file will contain the main application logic for the SkyModem application.
"""

from kuokka.kuokka.lib_receiver import ReceiverSettings, Receiver


settings = ReceiverSettings(sr0=1e6, baudrate=9600, bufferlen=600000, batch_maxlen=8000, f_tune=437.000e6, f_expected=437.020e6)
rx = Receiver(settings=settings)




