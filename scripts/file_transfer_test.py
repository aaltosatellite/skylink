"""
    Perform a file transfer test over the link.
"""

import os
import math
import time
import struct
import asyncio
import argparse
import binascii

from vc_connector import connect_to_vc, ARQTimeout, ReceptionTimeout


parser = argparse.ArgumentParser(description='Skylink test terminal')
parser.add_argument("role")
parser.add_argument('--filename', '-f', type=str)
parser.add_argument('--host', '-H', type=str, default="127.0.0.1")
parser.add_argument('--port', '-p', type=int, default=5000)
parser.add_argument('--vc', '-V', type=int, default=0)
parser.add_argument('--block_size', '-B', type=int, default=128)
parser.add_argument('--pp', action='store_false')
parser.add_argument('--rtt', action='store_true')
parser.add_argument('--rtt_init', action='store_true')
parser.add_argument('--arq', action='store_true')
parser.add_argument('--ack', action='store_true')
args = parser.parse_args()


#
# Open connection to Protocol implementation
#
conn = connect_to_vc(**vars(args))


async def transmit():

    await asyncio.sleep(1) # Wait for the ZMQ to connect

    f = open(args.filename, "rb")
    basename = os.path.basename(args.filename)

    local_crc = binascii.crc32(f.read())
    filesize = f.tell()
    f.seek(0, 0)

    block_count = int(math.ceil(filesize / args.block_size))

    print(f"Filesize: {filesize}")
    print(f"Block count: {block_count}")
    print(f"CRC: 0x{local_crc:08x}")

    #
    # Send init
    #
    if args.arq:
        await conn.arq_connect()
    await conn.transmit(struct.pack("!BII", ord("I"), filesize, local_crc) + basename.encode())
    rsp = await conn.receive(timeout=10)
    if rsp != b"OK":
        raise RuntimeError(f"Failed to init! {rsp!r}")


    #
    # Transfer all the blocks
    #
    for block_idx in range(0, block_count):

        # Make sure there's room in the buffer
        while True:
            if await conn.get_free() > 7 * 1024:
                break
            await asyncio.sleep(0.4)

        # Send a fragments to buffer
        print(f"Sending block #{block_idx}", await conn.get_free())

        f.seek(block_idx * args.block_size, 0)
        block_data = f.read(args.block_size)
        await conn.transmit(struct.pack("!BH", ord("T"), block_idx) + block_data)

        # Wait for
        try:
            rsp = await conn.receive(timeout=0.2)
            print("Received:", rsp)
        except ReceptionTimeout:
            pass # Ignore receive timeouts

async def receive():
    """
    Run file receiving loop
    """

    f = None
    remote_crc = 0
    block_count = 0
    start_time = 0
    prev_idx = 0

    while True:

        try:
            msg = await conn.receive()

        except ARQTimeout:
            print("ARQ Timeout")
            if f is not None:
                f.close()
            f = None
            continue


        if msg[0] == ord("I"):
            #
            # Init
            #

            _, filesize, remote_crc = struct.unpack("!BII", msg[0:9])
            block_count = int(math.ceil(filesize / args.block_size))
            filename = msg[9:].decode()
            await conn.transmit(b"OK")
            start_time = time.time()
            prev_idx = -1

            print(f"New file transfer {filename}, {filesize} bytes")

            f = open("downlinked_" + filename, "wb")
            f.truncate()
            f.write(b"\x00" * filesize)
            f.seek(0, 0)


        elif msg[0] == ord("T"):
            #
            # Transfer
            #

            if f is None:
                raise RuntimeError("No file open!")

            _, block_idx = struct.unpack("!BH", msg[0:3])
            print(f"Received {block_idx}")

            if args.arq:
                if block_idx != prev_idx + 1:
                    raise RuntimeError("")
                prev_idx = block_idx

            f.seek(block_idx * args.block_size, 0)
            f.write(msg[3:])

            if args.ack:
                await conn.transmit(struct.pack("!BH", ord("A"), block_idx))

            if block_idx == block_count - 1:
                print("Completed!")

                f.seek(0, 0)
                local_crc = binascii.crc32(f.read())
                print(f"Remote CRC: 0x{remote_crc:08x}")
                print(f"Local CRC:  0x{local_crc:08x}")

                print(f"Transfer rate: {filesize / (time.time() - start_time):.1s} bytes/s")



if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    async def run():
        try:
            if args.role == "transmit":
                await transmit()
            elif args.role == "receive":
                await receive()
            else:
                raise ValueError(f"Unknown role {args.role}")
        finally:
            conn.exit()

    loop.run_until_complete(run())
