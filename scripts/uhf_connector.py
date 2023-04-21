"""
    Virtual channel connector
"""
from __future__ import annotations

import struct
import asyncio
from typing import Any, Dict, Optional, Tuple, Union

from vc_connector import RTTChannel, ZMQChannel, SkylinkDevice, connect_to_vc
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

UHF_CMD_READ_VC0                = 0
UHF_CMD_READ_VC1                = 1
UHF_CMD_READ_VC2                = 2
UHF_CMD_READ_VC3                = 3
UHF_CMD_WRITE_VC0               = 4
UHF_CMD_WRITE_VC1               = 5
UHF_CMD_WRITE_VC2               = 6
UHF_CMD_WRITE_VC3               = 7
UHF_CMD_ARQ_CONNECT_VC0         = 8
UHF_CMD_ARQ_CONNECT_VC1         = 9
UHF_CMD_ARQ_CONNECT_VC2         = 10
UHF_CMD_ARQ_CONNECT_VC3         = 11
UHF_CMD_ARQ_DISCONNECT_VC0      = 12
UHF_CMD_ARQ_DISCONNECT_VC1      = 13
UHF_CMD_ARQ_DISCONNECT_VC2      = 14
UHF_CMD_ARQ_DISCONNECT_VC3      = 15
UHF_CMD_GET_SKY_STATE           = 16
UHF_CMD_GET_SKY_STATS           = 17
UHF_CMD_CLEAR_SKY_STATS         = 18
UHF_CMD_GET_SKY_CONFIGS         = 19
UHF_CMD_SET_SKY_CONFIG          = 20 # NOTE: Changes to sky config will apply only after reboot
UHF_CMD_PING                    = 21
UHF_CMD_GET_HOUSEKEEPING        = 22
UHF_CMD_CLEAR_STATS             = 23
UHF_CMD_COPY_CODE_TO_FRAM       = 24
UHF_CMD_GET_RADIO_CONFS         = 25
UHF_CMD_SET_RADIO_CONF          = 26
UHF_CMD_SWITCH_SIDE             = 27
UHF_CMD_SET_VOLATILE_TX_INHIBIT = 28
UHF_CMD_REBOOT                  = 29

# This are VC2 (services) only
UHF_CMD_SERVICE_SEND_TO_BUS             = 30
UHF_CMD_SERVICE_DEBUG_BEACON_ENABLE     = 31
UHF_CMD_SERVICE_DEBUG_BEACON_DISABLE    = 32
UHF_CMD_SERVICE_ECHO                    = 33

# This are VC3 (repeater) commands (over the bus only)
UHF_CMD_REPEATER_ENABLE                 = 34
UHF_CMD_REPEATER_DISABLE                = 35
UHF_CMD_REPEATER_CLEAR                  = 36
UHF_CMD_REPEATER_ADD_BEACON             = 37


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

RADIO_CONF_SIDE             = 0
RADIO_CONF_TX_INHIBIT       = 1
RADIO_CONF_SYMBOL_RATE_RX   = 2
RADIO_CONF_SYMBOL_RATE_TX   = 3

# Side A config
RADIO_CONF_FREQOFF0         = 4
RADIO_CONF_FREQOFF1         = 5
RADIO_CONF_PA_CFG2          = 6

# Side B config
RADIO_CONF_FREQOFF0         = 7
RADIO_CONF_FREQOFF1         = 8
RADIO_CONF_PA_CFG2          = 9

#
# UHF Radio Confs Repeater
#
REPEATER_CONF_BROADCAST_INTERVAL = 10
REPEATER_CONF_USER_REPETITIONS   = 11
REPEATER_CONF_REPEAT_INTERVAL    = 12

#
# Symbol rates
#

RADIO_GFSK_4800  = 1
RADIO_GFSK_9600  = 2
RADIO_GFSK_19200 = 3
RADIO_GFSK_38400 = 4

def display_radio_confs(confs: bytes):
    print(confs)
    confs = struct.unpack(">10BIII", confs)

    if confs[RADIO_CONF_SIDE] == 0:
        print("Side: A")
    else:
        print("Side B")
        
    if confs[RADIO_CONF_TX_INHIBIT] == 0:
        print("Tx inhibit: on")
    else:
        print("Tx inhibit: off")
    
    rate = [4800*2**i for i in range(0,4)]
    try:
        print(f"RX symbol rate: GFSK {rate[confs[RADIO_CONF_SYMBOL_RATE_RX]-1]}")
    except IndexError:
        print("RX symbol rate: unknown..")
    
    try:
        print(f"TX symbol rate: GFSK {rate[confs[RADIO_CONF_SYMBOL_RATE_TX]-1]}")
    except IndexError:
        print("TX symbol rate: unknown..")
        
    idx = RADIO_CONF_FREQOFF0
    print("\nSide A dynamic CC1125 confs:")
    print(f"    FREQOFF0: {confs[idx+0]:02x}")
    print(f"    FREQOFF1: {confs[idx+2]:02x}")
    print(f"    PA_CFG2:  {confs[idx+4]:02x}")

    print("\nSide B dynamic CC1125 confs:")
    print(f"    FREQOFF0: {confs[idx+1]:02x}")
    print(f"    FREQOFF1: {confs[idx+3]:02x}")
    print(f"    PA_CFG2:  {confs[idx+5]:02x}")


class ReceptionTimeout(Exception):
    pass

class ControlTimeout(Exception):
    pass

class ARQTimeout(Exception):
    pass


class UHFBusCommands:
    """
    General definitons of the control messages
    """
    def __init__(self, channel: Union[RTTChannel, ZMQChannel]) -> None:
        self.channel = channel

    async def get_state(self) -> SkyState:
        """
        Request virtual buffer status.

        Returns:
            SkyState object containting the buffer states
        Raises:
            `asyncio.exceptions.TimeoutError`
        """

        await self._send_command(UHF_CMD_GET_SKY_STATE)
        return parse_state(await self._wait_control_response(UHF_CMD_GET_SKY_STATE))


    async def sky_tx(self, vc: int, frame: Union[bytes, str]) -> None:
        """
        Send a frame to Virtual Channel buffer.

        Args:
            vc: virtual channel idx, frame: Frame to be send. `str`, `bytes` or `SkyFrame`
        """
        if isinstance(frame, str):
            frame = frame.encode('utf-8', 'ignore')
        await self._send_command(UHF_CMD_WRITE_VC0 + vc, frame)


    async def sky_rx(self, vc: int, timeout: Optional[float]=None) -> bytes:
        """
        Receive a frame to Virtual Channel buffer.

        Args:
            vc: virtual channel idx.
            timeout: Optional timeout time in seconds. None, receive will block forever.
            
        Raises:
            ReceptionTimeout in case of timeout.
        """

        await self._send_command(UHF_CMD_READ_VC0 + vc)
        return await self.channel.receive(timeout)
        
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
        rep = await self._wait_control_response(UHF_CMD_GET_RADIO_CONFS)
        print(rep)
        return rep
        
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
        await self._send_command(UHF_CMD_SET_VOLATILE_TX_INHIBIT)
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
        await self._send_command(UHF_CMD_ARQ_CONNECT_VC0 + vc)
        await self._wait_control_response(UHF_STATUS)


    async def arq_disconnect(self, vc: int):
        """
        ARQ disconnect
        """
        await self._send_command(UHF_CMD_ARQ_DISCONNECT_VC0 + vc)
        await self._wait_control_response(UHF_STATUS)


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
        await self._send_command(UHF_CMD_GET_SKY_STATS)
        return parse_stats(await self._wait_control_response(UHF_CMD_GET_SKY_STATS))


    async def clear_stats(self) -> None:
        """
        Clear Skylnk statistics
        """
        await self._send_command(UHF_CMD_CLEAR_SKY_STATS)

    
    async def _send_command(self, cmd: int, data: bytes = b""):
        await self.channel.transmit(bytes([cmd]) + data)

    async def _wait_control_response(self, expected_rsp: int) -> bytes:
        return await self.channel.receive_ctrl(expected_rsp)



if __name__ == "__main__":
    async def testing():
        channel = connect_to_vc(vc=2, rtt=True, device=SkylinkDevice.FS1p_UHF)
        vc = UHFBusCommands(channel)
        await asyncio.sleep(1) # Wait for connection

        await vc.copy_code_to_fram()
        #service_debug_beacon_on = struct.pack(">BBB", 0xAB, 1, 10)
        #await vc.transmit(2, service_debug_beacon_on)
        
        #print(await vc.get_housekeeping())
        #await vc.switch_volatile_tx_inhibit()

        display_radio_confs((c:=await vc.get_radio_confs()))

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

    async def test_skylink():
        channel = connect_to_vc(vc=2, rtt=True, rtt_init=False, device=SkylinkDevice.FS1p_UHF)
        uhf = UHFBusCommands(channel)
        await asyncio.sleep(1) # Wait for the connection

        #await uhf.arq_connect(vc=2)
        #await uhf.copy_code_to_fram()

        for i in range(500):
            await uhf.sky_tx(2, b'hellos'+ bytes(f'{i:03}'))
            await asyncio.sleep(0.1)


    loop = asyncio.get_event_loop()
    loop.run_until_complete(test_skylink())
