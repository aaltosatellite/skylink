"""
    Virtual channel connector
"""
from __future__ import annotations

import struct
import asyncio
from typing import Any, Dict, Optional, Tuple, Union

import zmq
import zmq.asyncio

from skylink import SkyState, SkyStatistics, parse_state, parse_stats

try:
    import pylink
except:
    pass # Allow starting without pylink installed


__all__ = [
    "RTTChannel",
    "ZMQChannel",
    "connect_to_vc"
]


#
# UHF Bus Commands
#


UHF_CMD_READ_VC0 = 0
UHF_CMD_READ_VC1 = 1
UHF_CMD_READ_VC2 = 2
UHF_CMD_READ_VC3 = 3

UHF_CMD_WRITE_VC0 = 4
UHF_CMD_WRITE_VC1 = 5
UHF_CMD_WRITE_VC2 = 6
UHF_CMD_WRITE_VC3 = 7

UHF_CMD_ARQ_WIPE_TO_OFF_STATE_VC0 = 8
UHF_CMD_ARQ_WIPE_TO_OFF_STATE_VC1 = 9
UHF_CMD_ARQ_WIPE_TO_OFF_STATE_VC2 = 10
UHF_CMD_ARQ_WIPE_TO_OFF_STATE_VC3 = 11

UHF_CMD_ARQ_WIPE_TO_INIT_STATE_VC0 = 12
UHF_CMD_ARQ_WIPE_TO_INIT_STATE_VC1 = 13
UHF_CMD_ARQ_WIPE_TO_INIT_STATE_VC2 = 14
UHF_CMD_ARQ_WIPE_TO_INIT_STATE_VC3 = 15

UHF_CMD_GET_SKY_STATE   = 16
UHF_CMD_GET_SKY_STATS   = 17
UHF_CMD_CLEAR_SKY_STATS = 18

UHF_CMD_GET_CONFIGS = 19
UHF_CMD_SET_CONFIG  = 20

UHF_CMD_PING = 21

UHF_CMD_GET_HOUSEKEEPING = 22

UHF_CMD_COPY_CODE_TO_FRAM = 23

UHF_CMD_GET_RADIO_CONFS = 24
UHF_CMD_SET_RADIO_CONF  = 25

UHF_CMD_SWITCH_SIDE = 26
UHF_CMD_SWITCH_VOLATILE_TX_INHIBIT = 27


#
# UHF Status Responses
#

UHF_STATUS = 69

UHF_STATUS_OK = 0
UHF_STATUS_ERR_NOT_IMPLEMENTED = -1
UHF_STATUS_ERR_UNKNOWN_COMMAND = -2

UHF_SKY_RET = 96

#
# UHF Radio Confs
#

RADIO_CONF_SIDE = 0
RADIO_CONF_TX_INHIBIT = 1
RADIO_CONF_SYMBOL_RATE_RX = 2
RADIO_CONF_SYMBOL_RATE_TX = 3

RADIO_CONF_FREQOFF0 = 4
RADIO_CONF_FREQOFF1 = 5
RADIO_CONF_PA_CFG2  = 6

#
# Symbol rates
#

RADIO_GFSK_4800  = 1
RADIO_GFSK_9600  = 2
RADIO_GFSK_19200 = 3
RADIO_GFSK_38400 = 4

def display_radio_confs(confs: bytes):
    confs = struct.unpack(">10B", confs)

    if confs[RADIO_CONF_SIDE] == 0:
        print("Side: A")
    else:
        print("Side B")
        
    if confs[RADIO_CONF_TX_INHIBIT] == 0:
        print("Tx inhibit: on")
    else:
        print("Tx inhibit: off")
        
    if confs[RADIO_CONF_SYMBOL_RATE_RX] == RADIO_GFSK_4800:
        print("RX symbol rate: GFSK 4800")
    elif confs[RADIO_CONF_SYMBOL_RATE_RX] == RADIO_GFSK_9600:
        print("RX symbol rate: GFSK 9600")
    elif confs[RADIO_CONF_SYMBOL_RATE_RX] == RADIO_GFSK_19200:
        print("RX symbol rate: GFSK 19200")
    elif confs[RADIO_CONF_SYMBOL_RATE_RX] == RADIO_GFSK_38400:
        print("RX symbol rate: GFSK 38400")
    else:
        print("Rx symbol rate: unknown..")

    if confs[RADIO_CONF_SYMBOL_RATE_TX] == RADIO_GFSK_4800:
        print("TX symbol rate: GFSK 4800")
    elif confs[RADIO_CONF_SYMBOL_RATE_TX] == RADIO_GFSK_9600:
        print("TX symbol rate: GFSK 9600")
    elif confs[RADIO_CONF_SYMBOL_RATE_TX] == RADIO_GFSK_19200:
        print("TX symbol rate: GFSK 19200")
    elif confs[RADIO_CONF_SYMBOL_RATE_TX] == RADIO_GFSK_38400:
        print("TX symbol rate: GFSK 38400")
    else:
        print("Tx symbol rate: unknown..")
        
    idx = RADIO_CONF_FREQOFF0
    print("\nSide A dynamic CC1125 confs:")
    print("    FREQOFF0: %x" % confs[idx+0])
    print("    FREQOFF1: %x" % confs[idx+2])
    print("    PA_CFG2:  %x" % confs[idx+4])

    print("\nSide B dynamic CC1125 confs:")
    print("    FREQOFF0: %x" % confs[idx+1])
    print("    FREQOFF1: %x" % confs[idx+3])
    print("    PA_CFG2:  %x" % confs[idx+5])


class ReceptionTimeout(Exception):
    pass

class ControlTimeout(Exception):
    pass

class ARQTimeout(Exception):
    pass


class BusCommands:
    """
    General definitons of the control messages
    """
    frame_queue: asyncio.Queue[bytes]
    control_queue: asyncio.Queue[Tuple[int, bytes]]

    async def get_state(self) -> SkyState:
        """
        Request virtual buffer status.

        Returns:
            SkyState object containting the buffer states
        Raises:
            `asyncio.exceptions.TimeoutError`
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        await self._send_command(UHF_CMD_GET_SKY_STATE)
        return parse_state(await self._wait_control_response(UHF_CMD_GET_SKY_STATE))


    async def transmit(self, vc: int, frame: Union[bytes, str]) -> None:
        """
        Send a frame to Virtual Channel buffer.

        Args:
            vc: virtual channel idx, frame: Frame to be send. `str`, `bytes` or `SkyFrame`
        """
        if isinstance(frame, str):
            frame = frame.encode('utf-8', 'ignore')
        await self._send_command(UHF_CMD_WRITE_VC0 + vc, frame)


    async def receive(self, vc: int, timeout: Optional[float]=None) -> bytes:
        """
        Receive a frame to Virtual Channel buffer.

        Args:
            vc: virtual channel idx.
            timeout: Optional timeout time in seconds. None, receive will block forever.
            
        Raises:
            ReceptionTimeout in case of timeout.
        """
        if self.frame_queue is None:
            raise RuntimeError("Channel closed")

        await self._send_command(UHF_CMD_READ_VC0 + vc)
        
        try:
            frame = await asyncio.wait_for(self.frame_queue.get(), timeout)
        except asyncio.TimeoutError as e:
            raise ReceptionTimeout() from e

        if isinstance(frame, ARQTimeout):
            raise frame
        return frame
        
    async def copy_code_to_fram(self):
        """
        Copy code to FRAM
        """
        await self._send_command(UHF_CMD_COPY_CODE_TO_FRAM)
        print(await self._wait_control_response(UHF_STATUS))
        
    async def get_housekeeping(self):
        """
        Get UHF housekeeping
        """
        await self._send_command(UHF_CMD_GET_HOUSEKEEPING)
        return await self._wait_control_response(UHF_CMD_GET_HOUSEKEEPING)
        
    async def get_radio_confs(self):
        """
        Get radio configs
        """
        await self._send_command(UHF_CMD_GET_RADIO_CONFS)
        return await self._wait_control_response(UHF_CMD_GET_RADIO_CONFS)
        
    async def set_radio_conf(self, config: int, val: int):
        """
        Set radio config
        """
        payload = struct.pack(">2B", config, val)
        await self._send_command(UHF_CMD_SET_RADIO_CONF, payload)
        print(await self._wait_control_response(UHF_STATUS))
        
    
    async def switch_volatile_tx_inhibit(self):
        """
        Switch tx inhibit on/off (nonvolatile)
        """
        await self._send_command(UHF_CMD_SWITCH_VOLATILE_TX_INHIBIT)
        print(await self._wait_control_response(UHF_STATUS))
        
    async def switch_side(self):
        """
    	Switch side
        """
        await self._send_command(UHF_CMD_SWITCH_SIDE)
        print(await self._wait_control_response(UHF_STATUS))
    
    async def arq_connect(self, vc: int):
        """
        ARQ connect
        """
        await self._send_command(UHF_CMD_ARQ_WIPE_TO_INIT_STATE_VC0 + vc)


    async def arq_disconnect(self, vc: int):
        """
        ARQ disconnect
        """
        await self._send_command(UHF_CMD_ARQ_WIPE_TO_OFF_STATE_VC0 + vc)


    async def get_free(self, vc: int) -> int:
        """
        Get number of free bytes in the virtual channel buffer.
        """
        status = await self.get_state()
        ret = status.vc[vc].buffer_free
        print("get_free returns:", ret)
        return ret


    async def get_stats(self) -> SkyStatistics:
        """
        Request protocol statistics.

        Returns:
            SkyStatistics object containing the statistics.
        Raises:
            `asyncio.exceptions.TimeoutError`
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        await self._send_command(UHF_CMD_GET_SKY_STATS)
        return parse_stats(await self._wait_control_response(UHF_CMD_GET_SKY_STATS))


    async def clear_stats(self) -> None:
        """
        Clear Skylnk statistics
        """
        await self._send_command(UHF_CMD_CLEAR_SKY_STATS)

class RTTChannel(BusCommands):
    """
    Connect to embedded Skylink implementation via Segger RTT.
    """

    def __init__(self, rtt_init: bool=False):
        """
        Initialize J-Link RTT server's Telnet connection.
        """
        self.frame_queue = asyncio.Queue()
        self.control_queue = asyncio.Queue()

        print(f"Connected to UHF via RTT")

        self.jlink = pylink.JLink()
        self.jlink.open()

        if rtt_init:
            self.jlink.set_tif(pylink.enums.JLinkInterfaces.JTAG)
            self.jlink.connect("VA10820")
            self.jlink.rtt_start()

        loop = asyncio.get_event_loop()
        self.task = loop.create_task(self.receiver_task())

    def exit(self):
        if self.task:
            self.task.cancel()
        self.task = None

    async def _read_exactly(self, num: int) -> bytes:
        """
        Read exactly N bytes from the RTT buffer.

        Args:
            num: Number of bytes to be read
        """
        buf: bytes = b""
        while len(buf) < num:
            ret: bytes = self.jlink.rtt_read(1, num - len(buf))
            if len(ret) == 0:
                await asyncio.sleep(0.01)
            else:
                buf += bytes(ret)
        return buf


    async def receiver_task(self) -> None:
        """
        Background task run receiver to receive and
        """

        sync = False
        while True:

            # Wait for frame sync word
            b = await self._read_exactly(1)

            if not self.jlink.connected():
                print("DISCONNECTED")
                await asyncio.sleep(0)
                break


            # Hunt the sync word
            if not sync:
                if b == b"\x5A":
                    sync = True

            else:
                if b == b"\xCE":
                    # Sync received, receive rest of the frame
                    hdr = await self._read_exactly(5)
                    data_len, _, _, cmd = struct.unpack(">HBBB", hdr)
                    
                    data = await self._read_exactly(data_len)

                    if UHF_CMD_READ_VC0 <= cmd <= UHF_CMD_READ_VC3:
                        await self.frame_queue.put(data)
                    else:
                        await self.control_queue.put((cmd, data))

                sync = False


        # Kill the queues
        qf, qc = self.frame_queue, self.control_queue
        self.frame_queue, self.control_queue = None, None
        await qf.put(None)
        await qc.put(None)


    async def _send_command(self, cmd: int, data: bytes = b""):
        """
        Send general command to RTT VC buffer.

        Args:
            cmd: Command code
            data:
        """
        if self.control_queue is None or not self.jlink.connected():
            raise RuntimeError("Channel closed")

        hdr = struct.pack(">BBHBBB", 0x5A, 0xCE, len(data), 0x69, 0x03, cmd)
        self.jlink.rtt_write(1, hdr + data)
        await asyncio.sleep(0) # For the function to behave as a couroutine


    async def _wait_control_response(self, expected_rsp: int) -> bytes:
        """
        Wait for specific control message.

        Args:
            expected_rsp: Wait until a control message with this code is received.

        Raises:
            ControlTimeout is raised if no response from Skylink is received in 200ms.
        """

        if self.control_queue is None or not self.jlink.connected():
            raise RuntimeError("Channel closed")

        try:
            while True:
                rsp_code, rsp_data = await asyncio.wait_for(self.control_queue.get(), timeout=2)
                if expected_rsp is None or rsp_code == expected_rsp:
                    return rsp_data
        except asyncio.TimeoutError as e:
            raise ControlTimeout() from e

if __name__ == "__main__":
    async def testing():

        vc = RTTChannel(False)
        await asyncio.sleep(1) # Wait for the ZMQ connection

        await vc.copy_code_to_fram()
        #service_debug_beacon_on = struct.pack(">BBB", 0xAB, 1, 10)
        #await vc.transmit(2, service_debug_beacon_on)
        
        #print(await vc.get_housekeeping())
        #await vc.switch_volatile_tx_inhibit()

        display_radio_confs(await vc.get_radio_confs())

        await vc.set_radio_conf(RADIO_CONF_SIDE,     1)
        await asyncio.sleep(1)
        await vc.set_radio_conf(RADIO_CONF_FREQOFF0, 255)
        await asyncio.sleep(1)
        await vc.set_radio_conf(RADIO_CONF_FREQOFF1, 1)
        await asyncio.sleep(1)
        await vc.set_radio_conf(RADIO_CONF_PA_CFG2,  0x43)        

        await asyncio.sleep(2)
        display_radio_confs(await vc.get_radio_confs())

        while True:
            #print(await vc.receive(0))
            #await vc.transmit(0, b"01234567")
            #print(await vc.get_state())
            #print(await vc.get_stats())
            print(await vc.get_housekeeping())
            await asyncio.sleep(2)

    loop = asyncio.get_event_loop()
    loop.run_until_complete(testing())
