#!/usr/bin/env python3
"""
    Skylink Test terminal
"""

import sys
import asyncio
import argparse

import zmq
import zmq.asyncio

parser = argparse.ArgumentParser(description='Skylink test terminal')
parser.add_argument('--port', '-p', type=int, default=5200)
parser.add_argument('--vc', '-V', type=int, default=0)
parser.add_argument('--binary', '-B', action='store_true')
parser.add_argument('--pp', action='store_false')
args = parser.parse_args()



print(f"Test terminal: RX {args.port+args.vc}, TX {args.port+args.vc+100}")
ctx = zmq.asyncio.Context()

# Open downlink socket
dl = ctx.socket(zmq.PULL if args.pp else zmq.SUB)
dl.connect("tcp://localhost:%d" % (args.port + args.vc))
if not args.pp:
    dl.setsockopt(zmq.SUBSCRIBE, b"")

# Open uplink socket
ul = ctx.socket(zmq.PUSH if args.pp else zmq.PUB)
ul.connect("tcp://localhost:%d" % (args.port + args.vc + 100))
ul.setsockopt(zmq.SNDHWM, 10) # Set high water mark for outbound messages


input_line = ""

def print(*args, **kwargs):
    """
    Print to stdout and
    """
    global input_line
    sys.stdout.write("\r    " + (" " * len(input_line)) + "\r")
    sys.stdout.write(" ".join(args))
    sys.stdout.write("\r\n")
    sys.stdout.write(">>> " + input_line)
    sys.stdout.flush()


async def test_counter():

    i = 0
    while True:
        await asyncio.sleep(1)
        print(f"RX: {i}")
        i += 1


async def dl_loop():
    """
    Coroutine to wait and print incoming frames
    """
    while True:
        msg = await dl.recv()
        if args.binary:
            sys.stdout.buffer.write(msg)
            sys.stdout.buffer.flush()
        else:
            print(f"RX: {msg!r}")


async def up_loop():
    """
    Coroutine to read stdin and send frames out
    """
    global input_line
    loop = asyncio.get_event_loop()

    import sys, tty, termios
    tty.setcbreak(sys.stdin.fileno())

    if 0:
        fd = sys.stdin.fileno()
        oldattr = termios.tcgetattr(fd)
        newattr = oldattr[:]

        if 0: # echo
            # disable ctrl character printing, otherwise, backspace will be printed as "^?"
            lflag = ~(termios.ICANON | termios.ECHOCTL)
        else:
            lflag = ~(termios.ICANON | termios.ECHO)
        newattr[3] &= lflag
        termios.tcsetattr(fd, termios.TCSADRAIN, newattr)

    print()

    try:
        while True:

            if args.binary:
                msg = await sys.stdin.buffer.read()
                await ul.send(msg)
            else:

                # Wait for line
                while True:
                    c = await loop.run_in_executor(None, sys.stdin.read, 1)

                    if c == "\n":
                        #sys.stdout.write("\n")
                        break
                    elif ord(c) == 127: # Backspace
                        input_line = input_line[:-1]
                        sys.stdout.write("\b \b")
                    else:
                        input_line += c

                    sys.stdout.write(c)
                    sys.stdout.flush()

            msg, input_line = input_line, ""

            if len(msg) > 0:
                print(f"TX: {msg}")
                await ul.send(msg.encode('utf-8', 'ignore'))

    except KeyboardInterrupt:
        loop.stop()
    #finally:


if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.create_task(dl_loop())
    #loop.create_task(test_counter())
    loop.create_task(up_loop())
    loop.run_forever()
