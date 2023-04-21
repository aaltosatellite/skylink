# Test script for VC_connector.py
# Spam message to test if it can be received properly

import datetime
import json
import struct
from typing import NamedTuple
import numpy as np
import zmq
import zmq.asyncio

import asyncio

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


async def test_suo_skylink():
    from vc_connector import ZMQChannel, ReceptionTimeout     # pylint: disable=import-outside-toplevel
    from uhf_connector import test_skylink  # pylint: disable=import-outside-toplevel

    pkt_count = 500


    rx = ZMQChannel("127.0.0.1", 7100, vc=2)
    rx.config['show_delay'] = True

    await test_skylink(pkt_count, 0.2)

    class TestPacket(NamedTuple):
        number: int
        delay: int

    rx_packets: list[TestPacket] = []

    try:
        while (pkt := await rx.receive(0.5)) is not None:
            rx_packets.append(TestPacket(*struct.unpack('>6xHI', pkt)))
    except ReceptionTimeout:
        pass

    if len(rx_packets) != 0:
        data = np.array(rx_packets).transpose()

        print(f'received count: {len(data[0])}/{pkt_count}')
        print(f'avg delay:      {np.mean(data[1])}')
    else:
        print('No packets received')


if __name__ == '__main__':
    
    loop = asyncio.get_event_loop()
    loop.run_until_complete(test_suo_skylink())


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