
# Test script for VC_connector.py
# Spam message to test if it can be received properly

import datetime
import json
import struct
from typing import NamedTuple
import zmq
import zmq.asyncio

import numpy as np
from matplotlib import pyplot
from pandas import DataFrame

import asyncio


from vc_connector import RTTChannel, SkylinkDevice, ZMQChannel, ReceptionTimeout       # pylint: disable=import-outside-toplevel
from uhf_connector import suo_rx_skylink, suo_tx_skylink                      # pylint: disable=import-outside-toplevel

zmq.asyncio.Context()


async def wait_a_bit(a_bit: int = 3):
    await asyncio.sleep(0.5)

    for _ in range(a_bit):
        print(".", end='')
        await asyncio.sleep(0.5)

    print()


async def send_pkt(socket: zmq.asyncio.Context, data) -> None:
    print("sending packet:", data)

    await socket.send(data)


async def spam_test(sock):
    print("waiting", end='')

    await wait_a_bit(5)

    base_pkt = {"data":"hellos",
                "metadata":{"vc":2},
                "packet_type":"downlink",
                "timestamp": None,
                "vc":2
                }

    for i in range(50):
        ts = datetime.datetime.now().isoformat()
        base_pkt['data'] = (b'hellos'+ struct.pack('>H', i)).hex()
        base_pkt["timestamp"] = ts+'Z'
        pkt = bytes(json.dumps(base_pkt), 'ascii')

        await send_pkt(sock, pkt)
        await asyncio.sleep(0.2)

def spam_test_full():
    ctx = zmq.asyncio.Context()
    host = "127.0.0.1"
    port = 7100
    vc = 2

    ul = ctx.socket(zmq.PUB)
    ul.bind((ul_port := f"tcp://{host}:{port + vc*10}"))
    ul.setsockopt(zmq.SNDHWM, 10) # Set high water mark for outbound messages

    print("connected test to:", ul_port)

    loop = asyncio.get_event_loop()
    loop.run_until_complete(spam_test(ul))


async def test_suo_rx_skylink():

    pkt_count = 500


    rx = ZMQChannel("127.0.0.1", 7100, vc=2)
    rx.config['show_delay'] = True

    await suo_rx_skylink(pkt_count)

    class TestPacket(NamedTuple):
        number: int
        delay: int

    rx_packets: list[TestPacket] = []

    try:
        while (pkt := await rx.receive(2)) is not None:
            print(pkt)
            print((rx_tup := struct.unpack('>7xHxI', pkt)))
            rx_packets.append(TestPacket(*rx_tup))
    except ReceptionTimeout:
        print('Reception timed out')
        pass

    data = DataFrame(rx_packets, columns=['pkt_num', 'delay'])
    fn = f"vc_connector_tests/vc_connector_test-{datetime.datetime.now().isoformat()}.txt"
    data.to_csv(fn, index=False)

    if len(rx_packets) != 0:

        print(f'received count: {len(data["pkt_num"])}/{pkt_count}')
        print(f'mean   delay:   {data["delay"].mean()}')
        print(f'median delay:   {data["delay"].median()}')

        
        data.plot(x='pkt_num',y='delay')
    else:
        print('No packets received')


async def test_suo_tx_skylink():
    #uhf = RTTChannel(2, device=SkylinkDevice.FS1p_UHF, print_trx=True)
    #uhf.config['show_delay'] = True

    pkt_count = 10

    await suo_tx_skylink(pkt_count, arq=False)



if __name__ == '__main__':
    
    loop = asyncio.get_event_loop()
    loop.run_until_complete(test_suo_tx_skylink())


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

Testing starting all tests at once
 - Start skymodem
 - Start vc_connector.py:test_skylink_rx()
 - Start uhf_connector.py:test_skylink()

We can now get results from this script... MAKE PLOTS!?

"""