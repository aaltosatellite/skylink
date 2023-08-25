# Test script for VC_connector.py
# Spam message to test if it can be received properly

import datetime
import struct
from typing import NamedTuple
import zmq
import zmq.asyncio

import matplotlib.pyplot as plt
from pandas import DataFrame

import asyncio

from vc_connector import RTTChannel, SkylinkDevice, ZMQChannel, ReceptionTimeout       # pylint: disable=import-outside-toplevel
from uhf_connector import uhf_to_sdr, sdr_to_uhf                                       # pylint: disable=import-outside-toplevel

import sys
import logging

import pylink
import importlib

zmq.asyncio.Context()



class RxPacket(NamedTuple):
    number: int
    delay: int

async def test_downlink_rtt(log: bool = False, plot: bool = False):

    tx_pkt_count = 200

    await uhf_to_sdr(tx_pkt_count, arq = False)

    rx = ZMQChannel("127.0.0.1", 7100, vc=2)
    rx.config['show_delay'] = True

    rx_packets: list[RxPacket] = []

    try:
        while (rx_pkt := await rx.receive(2)) is not None:
            rx_tup = struct.unpack('>9xHxI', rx_pkt)
            print(rx_tup)
            rx_packets.append(RxPacket(*rx_tup))
    except ReceptionTimeout:
        logging.info('Reception timed out')
        pass

    data = DataFrame(rx_packets, columns=['pkt_num', 'delay'])

    if len(rx_packets) != 0:
        logging.info(f'Received packets:   {len(data["pkt_num"])}/{tx_pkt_count} ({format(len(data["pkt_num"])/tx_pkt_count * 100, ".1f")} %)')
        logging.info(f'Mean delay (ms):    {format(data["delay"].mean()/1e3, ".3f")}')
        logging.info(f'Median delay (ms):  {format(data["delay"].median()/1e3, ".3f")}')
        
        if log:
            result_fn = f"skylink_test_results/downlink_test_result-{datetime.datetime.now().isoformat()}.txt"
            data.to_csv(result_fn, index=False)

        if plot:
            plt.scatter(data["pkt_num"], data["delay"]/1e3,marker='X')
            plt.title(f'Delay of successfully received packets ({len(data["pkt_num"])}/{tx_pkt_count} packets received)')
            plt.xlabel("Packet number")
            plt.ylabel("Delay (ms)")
            plt.xlim([0,tx_pkt_count])
            plt.grid()
            plt.show()

    else:
        logging.info('No packets received')



async def test_uplink_rtt(log: bool = False, plot: bool = False):

    tx_pkt_count = 200

    await sdr_to_uhf(tx_pkt_count, arq=False)

    jlink = pylink.JLink()
    jlink.open()
    jlink.set_tif(pylink.enums.JLinkInterfaces.JTAG)
    jlink.connect("va10820")
    print("Connected, starting RTT...")
    jlink.rtt_start(None)

    while True:
        try:
            num_up = jlink.rtt_get_num_up_buffers()
            num_down = jlink.rtt_get_num_down_buffers()
            print("RTT started, %d up bufs, %d down bufs." % (num_up, num_down))
            break
        except pylink.errors.JLinkRTTException:
            time.sleep(0.1)
    
    # Read received packets from RTT
    full_str = ""
    terminal_bytes = jlink.rtt_read(3, 256)
    while terminal_bytes:
        full_str = full_str + ("".join(map(chr, terminal_bytes)))
        time.sleep(0.1)
        terminal_bytes = jlink.rtt_read(3, 256)

    rx_payloads = full_str.split(",")[:-1]
    rx_packets: list[RxPacket] = []

    if len(rx_payloads) != 0:
        for payload in rx_payloads:
            # Extract the last bytes of the payload to get the payload number
            n = 2
            split_bytes_string_array = [payload[i:i+n] for i in range(0, len(payload), n)]
            payload_num = int(split_bytes_string_array[-2],16) + int(split_bytes_string_array[-1],16)
            delay = 0 # TODO
            rx_tup = (payload_num,delay)
            rx_packets.append(RxPacket(*rx_tup))
        
        data = DataFrame(rx_packets, columns=['pkt_num', 'delay'])
        logging.info(f'Received packets:   {len(rx_payloads)}/{tx_pkt_count} ({format(len(data["pkt_num"])/tx_pkt_count * 100, ".1f")} %)')

        if log:
            result_fn = f"skylink_test_results/uplink_test_result-{datetime.datetime.now().isoformat()}.txt"
            data.to_csv(result_fn, index=False)

        if plot:
            plt.scatter(data["pkt_num"], data["delay"]/1e3,marker='X')
            plt.title(f'Delay of successfully received packets ({len(data["pkt_num"])}/{tx_pkt_count} packets received)')
            plt.xlabel("Packet number")
            plt.ylabel("Delay (ms)")
            plt.xlim([0,tx_pkt_count])
            plt.grid()
            plt.show()
    
    else:
        logging.info("No packets received") 


async def test_downlink_egse(log: bool = False, plot: bool = False):

    tx_pkt_count = 200

    await uhf_to_sdr(tx_pkt_count, rtt = False, arq = False)

    rx = ZMQChannel("127.0.0.1", 7100, vc=2)
    rx.config['show_delay'] = True

    rx_packets: list[RxPacket] = []

    try:
        while (rx_pkt := await rx.receive(2)) is not None:
            rx_tup = struct.unpack('>9xHxI', rx_pkt)
            print(rx_tup)
            rx_packets.append(RxPacket(*rx_tup))
    except ReceptionTimeout:
        logging.info('Reception timed out')
        pass

    data = DataFrame(rx_packets, columns=['pkt_num', 'delay'])

    if len(rx_packets) != 0:
        logging.info(f'Received packets:   {len(data["pkt_num"])}/{tx_pkt_count} ({format(len(data["pkt_num"])/tx_pkt_count * 100, ".1f")} %)')
        logging.info(f'Mean delay (ms):    {format(data["delay"].mean()/1e3, ".3f")}')
        logging.info(f'Median delay (ms):  {format(data["delay"].median()/1e3, ".3f")}')
        
        if log:
            result_fn = f"skylink_test_results/downlink_test_result-{datetime.datetime.now().isoformat()}.txt"
            data.to_csv(result_fn, index=False)

        if plot:
            plt.scatter(data["pkt_num"], data["delay"]/1e3,marker='X')
            plt.title(f'Delay of successfully received packets ({len(data["pkt_num"])}/{tx_pkt_count} packets received)')
            plt.xlabel("Packet number")
            plt.ylabel("Delay (ms)")
            plt.xlim([0,tx_pkt_count])
            plt.grid()
            plt.show()

    else:
        logging.info('No packets received')


async def test_uplink_egse(log: bool = False, plot: bool = False):

    tx_pkt_count = 200

    await sdr_to_uhf(tx_pkt_count, arq=False)

    egse()
    
    # Read received packets from RTT
    full_str = ""
    terminal_bytes = jlink.rtt_read(3, 256)
    while terminal_bytes:
        full_str = full_str + ("".join(map(chr, terminal_bytes)))
        time.sleep(0.1)
        terminal_bytes = jlink.rtt_read(3, 256)

    rx_payloads = full_str.split(",")[:-1]
    rx_packets: list[RxPacket] = []

    if len(rx_payloads) != 0:
        for payload in rx_payloads:
            # Extract the last bytes of the payload to get the payload number
            n = 2
            split_bytes_string_array = [payload[i:i+n] for i in range(0, len(payload), n)]
            payload_num = int(split_bytes_string_array[-2],16) + int(split_bytes_string_array[-1],16)
            delay = 0 # TODO
            rx_tup = (payload_num,delay)
            rx_packets.append(RxPacket(*rx_tup))
        
        data = DataFrame(rx_packets, columns=['pkt_num', 'delay'])
        logging.info(f'Received packets:   {len(rx_payloads)}/{tx_pkt_count} ({format(len(data["pkt_num"])/tx_pkt_count * 100, ".1f")} %)')

        if log:
            result_fn = f"skylink_test_results/uplink_test_result-{datetime.datetime.now().isoformat()}.txt"
            data.to_csv(result_fn, index=False)

        if plot:
            plt.scatter(data["pkt_num"], data["delay"]/1e3,marker='X')
            plt.title(f'Delay of successfully received packets ({len(data["pkt_num"])}/{tx_pkt_count} packets received)')
            plt.xlabel("Packet number")
            plt.ylabel("Delay (ms)")
            plt.xlim([0,tx_pkt_count])
            plt.grid()
            plt.show()
    
    else:
        logging.info("No packets received") 


async def egse():
    
    from foresail1p.egse import interfaces
    
    print("Connecting to EGSE")
    interfaces.connect_to_bus(method="usb", device="/dev/ttyUSB2")
    #interfaces.connect_to_bus(method="skylink")

    egse = { }
    modules = [ "obc", "eps", "uhf", "mtq", "cam", "matti" ]
    for module_name in modules:
        egse[module_name] = importlib.import_module(f".{module_name}", package="foresail1p.egse")

    egse = type('EGSE', (), egse)() # Don't ask what happens here. t: Petri

    return egse





if __name__ == '__main__':
    import time
    time.sleep(0)
    log = False
    plot = False

    if ("u" not in sys.argv) and ("d" not in sys.argv):
        print("Specify either downlink or uplink test by parameters 'u' or 'd', respectively")
    
    else:
        if "l" in sys.argv:
            log = True
        if "p" in sys.argv:
            plot = True
        if "d" in sys.argv:
            downlink = True
            log_fn = f'skylink_test_logs/downlink_test_log-{datetime.datetime.now().isoformat()}.log'
        if "u" in sys.argv:
            downlink = False
            log_fn = f'skylink_test_logs/uplink_test_log-{datetime.datetime.now().isoformat()}.log'
            
        logger = logging.getLogger()
        logger.setLevel(logging.INFO)

        console_fh = logging.StreamHandler(sys.stdout)
        console_fh.setLevel(logging.INFO)
        console_formatter = logging.Formatter('%(message)s')
        console_fh.setFormatter(console_formatter)
        logger.addHandler(console_fh)

        if log: # Log data to a file
            log_fh = logging.FileHandler(log_fn)
            log_fh.setLevel(logging.INFO)
            log_formatter = logging.Formatter('%(asctime)s.%(msecs)03d: %(message)s',datefmt=("%d.%m.%Y %H:%M:%S"))
            log_fh.setFormatter(log_formatter)
            logger.addHandler(log_fh)

        loop = asyncio.get_event_loop()
        if downlink:
            logging.info("Testing DOWNLINK")
            loop.run_until_complete(test_downlink_rtt(log, plot))
        else:
            logging.info("Testing UPLINK")
            loop.run_until_complete(test_uplink_rtt(log, plot))


"""
------------------------------------------------------

Setup:
Direct connection UHF <-> B200
UHF tx power        minimum/default 
Skymodem rx gain    60 dB

Attenuation         90 dB
gqrx gain           50 dB
UHF Txr            -50 dB

Notes:
Corrupt messages only until PA reaches ~28 degrees

------------------------------------------------------

Setup:
Direct connection UHF <-> B200
UHF tx power        minimum/default 
Skymodem rx gain    40 dB

Attenuation         60 dB
gqrx gain           15 dB
UHF Txr            -50 dB

Notes:
All messages corrupt regardless of PA temp


------------------------------------------------------

Setup:
Direct connection UHF <-> B200
UHF tx power        minimum/default 
Skymodem rx gain    40 dB

Attenuation         80 dB
gqrx gain           40 dB
UHF Txr            -50 dB

Notes:
This seems to be the link budget (power-attenuation-gain) sweetspot.
Messages are still coming corrupted but many more get through
At first, many messages get through, and later this starts to decay.



Just run vc_connector_test.py for testing.



"""
