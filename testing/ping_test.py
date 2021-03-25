"""
    Perform a ping/pong test over the link.
"""

import time
import asyncio
import argparse
from vc_connector import RTTChannel, ZMQChannel


parser = argparse.ArgumentParser(description='Skylink test terminal')
parser.add_argument('role', type=str)
parser.add_argument('--host', '-H', type=str, default="127.0.0.1")
parser.add_argument('--port', '-p', type=int, default=5200)
parser.add_argument('--vc', '-V', type=int, default=0)
parser.add_argument('--pp', action='store_false')
parser.add_argument('--rtt', action='store_true')
args = parser.parse_args()


#
# Connect to protocol virtual channel
#
if args.rtt:
    if args.port == 5200:
        args.port = 19021
    conn = RTTChannel(args.host, args.port, vc=args.vc)
else:
    conn = ZMQChannel(args.host, args.port, vc=args.vc, pp=args.pp)


async def pinger():
    """
        Ping the other end
    """
    i = 0
    while True:

        # Send a PING frame
        t = time.time()
        await conn.transmit(b"PING%d" % i)

        # Wait for corresponding PONG frame
        while True:
            resp = await conn.receive(timeout=20)
            print(resp)
            if resp.startswith(b"PONG") and int(resp[4:]) == i:
                print(f"Latency {time.time() - t} s")
                break

        asyncio.sleep(0.5)


async def ponger():
    """
        Respond to incoming pings
    """

    while True:
        frame = await conn.receive(timeout=20)

        if frame.startswith(b"PING"):
            print(frame)
            rsp = b"PONG" + frame[4:]
            await conn.transmit(rsp)



if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    if args.role == "ping":
        coro = pinger
    elif args.role == "pong":
        coro = ponger
    else:
        raise ValueError(f"Unkmown role {args.role}")

    loop.run_until_complete(coro())
