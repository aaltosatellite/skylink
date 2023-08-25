"""
    Perform a ping/pong test over the link.
"""

import time
import asyncio
import argparse
import random
from vc_connector import connect_to_vc, ReceptionTimeout, ARQTimeout


parser = argparse.ArgumentParser(description='Skylink test terminal')
parser.add_argument('role', type=str)
parser.add_argument('--host', '-H', type=str, default="127.0.0.1")
parser.add_argument('--port', '-p', type=int, default=7100)
parser.add_argument('--vc', '-V', type=int, default=0)
parser.add_argument('--pp', action='store_false')
parser.add_argument('--rtt', action='store_true')
parser.add_argument('--rtt_init', action='store_true')
parser.add_argument('--rate', '-r', type=float, default=1, help="Ping transmission rate")
parser.add_argument('--payload', type=int, default=0, help="Number of addiotional payload bytes")
parser.add_argument('--arq', action='store_true', help="Use Automatic Retransmission")
parser.add_argument('--max_on_air', type=int, default=8, help="Maximum number of commands on air simultaneously")

args = parser.parse_args()


#
# Connect to protocol virtual channel
#
conn = connect_to_vc(**vars(args))


def random_bytes(l: int) -> bytes:
    return bytes([ random.randint(0, 255) for _ in range(l) ])

async def pinger():
    """
    Generate new ping frames at the given rate. Allow multiple pings command to be in air simultanously.
    When pong/answer is received calculate the end-to-end latency for the specific ping.
    """
    await asyncio.sleep(0.3) # Wait for the ZMQ to connect

    on_air = { }

    # Initialize ARQ
    if args.arq:
        await conn.arq_connect()

    async def generate_pings():
        i = 0
        while True:
            await asyncio.sleep(args.rate)

            # Limit the maximum number of pings on the air
            if args.arq and len(on_air) > args.max_on_air:
                continue

            # Send a PING frame
            on_air[i] = time.time()
            await conn.transmit((b"PING%d " % i) + random_bytes(args.payload))
            i += 1

    loop.create_task(generate_pings())

    while True:
        # Wait for corresponding PONG frame
        frame = await conn.receive(timeout=None)

        if frame.startswith(b"PONG"):
            id = int(frame[4:frame.index(b" ")])
            if id in on_air:
                print(f"{id} latency {1000 * (time.time() - on_air[id]):.0f} ms")
                del on_air[id]
            else:
                print("RESET!")
                on_air = {} # Reset

        else:
            print(frame)



async def ponger():
    """
    Respond immiditately to incoming pings with pong packet.
    """

    while True:
        try:
            frame = await conn.receive(timeout=4)
            if frame.startswith(b"PING"):
                id = int(frame[4:frame.index(b" ")])
                print("PING", id)
                rsp = (b"PONG%d " % id) + random_bytes(args.payload)
                await conn.transmit(rsp)
            else:
                print(frame)

        except ARQTimeout:
            print("ARQTimeout")

        except ReceptionTimeout:
            print(".", end="", flush=True)



if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    async def run():
        try:
            if args.role == "ping":
                await pinger()
            elif args.role == "pong":
                await ponger()
            else:
                raise ValueError(f"Unknown role {args.role}")
        finally:
            conn.exit()

    loop.run_until_complete(run())
