# Test script for VC_connector.py
# Spam message to test if it can be received properly

import datetime
import json
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

    base_pkt = {"data":"test",
                "metadata":{"vc":2},
                "packet_type":"downlink",
                "timestamp": None,
                "vc":2
                }

    for i in range(50):
        ts = datetime.datetime.now().isoformat()
        base_pkt['data'] = f'test{i:03}'
        base_pkt["timestamp"] = ts
        pkt = bytes(json.dumps(base_pkt), 'utf-8')

        await send_pkt(ul, pkt)
        await asyncio.sleep(0.2)


if __name__ == '__main__':
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
