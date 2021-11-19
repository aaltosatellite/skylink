#!/usr/bin/env python3
"""
    Skylink Test terminal
"""

import sys
import asyncio
import argparse

from vc_connector import connect_to_vc


parser = argparse.ArgumentParser(description='Skylink test terminal')
parser.add_argument('--host', '-H', type=str, default="127.0.0.1")
parser.add_argument('--port', '-p', type=int, default=5000)
parser.add_argument('--vc', '-V', type=int, default=0)
parser.add_argument('--binary', '-B', action='store_true')
parser.add_argument('--pp', action='store_false')
parser.add_argument('--rtt', action='store_true')
args = parser.parse_args()

"""
Connect to Skylink
"""
connector = connect_to_vc(**vars(args))


input_line = ""

def print(*args, **kwargs):
    """
    Print to stdout and
    """
    global input_line
    sys.stdout.write("\r    " + (" " * len(input_line)) + "\r")
    sys.stdout.write(" ".join(map(str, args)))
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
        msg = await connector.receive()
        if msg is None:
            return

        if args.binary:
            sys.stdout.buffer.write(msg)
            sys.stdout.buffer.flush()
        else:
            print(f"RX: {msg!r}")


async def execute_command(cmd: str):
    """
    Execute control command from the command line.
    All the commands must start with `!`.
    """
    try:
        if cmd == "!B":
            print(await connector.get_status())
        elif cmd == "!f":
            print("Free in TX buffer:", await connector.get_free())
        elif cmd == "!S":
            print(await connector.get_stats())
        else:
            print("Unknown command")

    except asyncio.exceptions.TimeoutError:
        print("! Timeout")


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
                msg = await loop.run_in_executor(None, sys.stdin.read)

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

                msg, input_line = input_line.strip(), ""

            if len(msg) > 0:
                if msg.startswith("!"):
                    await execute_command(msg)
                else:
                    print(f"TX: {msg}")
                    await connector.transmit(msg)

    except KeyboardInterrupt:
        loop.stop()

    finally:
        #termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, oldattr)
        pass


if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.create_task(dl_loop())
    #loop.create_task(test_counter())
    loop.run_until_complete(up_loop())
