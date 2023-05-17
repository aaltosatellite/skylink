# SkyModem JSON interface

For communication with the mission control system Skymodem uses JSON formatted packets over ZMQ PUB/SUB sockets. (Implementation of the interface can be found from: skymodem/vc_interface.cpp.) By default, skymodem will create a new binding ZMQ socket for each virtual channel and communication direction (tx/rx). Skymodem subcribes all messages incoming to rx socket and published responses and other asyncronoush messages such as received packets to tx automatically.

The ZMQ port numbers are numbered using following equation `base_port + 10 * vc_index + (tx_port ? 1 : 0)`. For example, if the base_port is 7100 (default value) the application software using Skylink sends the commands to port 7111 to use VC1 and command responses are on port 7110.


| Port Number | Description             |
| ----------- | ----------------------- |
|    7100     | VC0 Receive/Downlink    |
|    7101     | VC0 Transmit/Uplink     |
|    7110     | VC1 Receive/Downlink    |
|    7111     | VC1 Transmit/Uplink     |
|    7120     | VC2 Receive/Downlink    |
|    7121     | VC2 Transmit/Uplink     |
|    7130     | VC3 Receive/Downlink    |
|    7132     | VC3 Transmit/Uplink     |



# JSON packet formats

## Transmitting a packet

To transmit (aka push a new packet to Skylink's buffer where Skylink will transmit it) following message
```
{
    "packet_type": "tc",
    "data": "123456789abcdef",
    "vc": 0,
}
```

Where:
- `packet_type` is `tc` to indicate that this is telecommand packet.
- The `data` field is a hexadecimal string of the bytes to transmitted.
- `vc` is optional virtual channel index. If the field is given the frame is pushed to specified virtual channel else virtual channel by the used port number is used.


## Receiving packet

When ever a frame is received by the modem and Skylink accepts it, skymodem will automatically transfer the packet to publishing virtual channels socket.

```
{
    "packet_type": "tm",
    "timestamp": "2023-04-20T12:01:00Z",
    "data": "123456789abcdef",
    "vc": 1,
    "metadata": { }
}
```

Where:
- `packet_type` is always `tm` to indicate that this is telemetry packet.
- `timestamp` is ISO 8601 datetime string in UTC timezone.
- The `data` field is a hexadecimal string of the bytes to transmitted.
- `vc` is the virtual channel number.
- `metadata` is a dict to containing possible other metadata.


## ARQ connected/timeout messages

When the ARQ process changes state following message is output publish socket.

```
{
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "vc": 0,
    "metadata": {
        "rsp": "arq_connected" | "arq_timeout"
        "vc": 0,
        "session_identifier": 123
    }
}
``` 


## Control commands:

### Get status

Command:
```
{
    "packet_type": "control",
    "metadata": {
        "cmd:" "get_state"
    }
}
```

Response:
```
{ 
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "metadata": { 
        "cmd:" "state",
        "state": [
            { "arq_state": 2, "buffer_free": 8, "tx_frames": 0, "rx_frames": 0 },
            { "arq_state": 0, "buffer_free": 10, "tx_frames": 0, "rx_frames": 0 },
            { "arq_state": 0, "buffer_free": 10, "tx_frames": 0, "rx_frames": 0 },
            { "arq_state": 0, "buffer_free": 10, "tx_frames": 0, "rx_frames": 0 },
        ]
    }
}
``` 

### Get statistics

Command:
```
{
    "packet_type": "control",
    "metadata": { 
        "rsp:" "get_stats",
    }
}
```

Response:
```
{
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "metadata": {
        "cmd:" "get_state", 
        "rx_frames": 0,
        "rx_arq_resets": 0,
        "tx_frames": 0,
        "tx_bytes": 0
    }
}
``` 


### Clear statistics

Command:
```
{
    "packet_type": "control",
    "metadata": { 
        "rsp:" "clear_stats",
    }
}
```

No response


### Set config

Command:
```
{
    "packet_type": "control",
    "metadata": {
        "cmd": "set_config",
        "config": "blaa",
        "value": 123 
    }
}
```

No response


### Get config

Command:
```
{ 
    "packet_type": "control",
    "metadata": { 
        "cmd": "get_config",
        "config": "blaa"
    }
}
```

Response:
```
{
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "metadata": {
        "rsp": "config",
        "config": "blaa",
        "value": 123
    }
}
``` 



### ARQ connect

```
{ 
    "packet_type": "control",
    "metadata": {
        "cmd:" "arq_connect"
    }
}
```

Response:
```
{
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "metadata": {
        "rsp": "arq_connecting",
        "session_identifier": 123,
    }
}
``` 


### ARQ disconnect

```
{
    "packet_type": "control",
    "metadata": {
        "cmd": "arq_disconnect"
    }
}
``` 


### MAC reset

```
{
    "packet_type": "control",
    "metadata": {
        "cmd": "arq_disconnect"
    }
}
``` 



