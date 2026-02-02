import threading
from dsp_library.kuokka.radio_loop import RadioLoop, RadioConfig
from dsp_library.kuokka.dsp_loop import DSPLoop, TXDSPConfig
from dsp_library.kuokka.lib_receiver import RXDSPConfig
from skylink_wrapper.cython_skylink import SkyLinkLoop, SkyConfiguration, EKEY_SKY_ARQ_DISCONNECTED, EKEY_SKY_PAYLOAD, EKEY_SKY_ARQ_CONNECTED
from skylink_wrapper.cython_skylink import num_virtual_channels, arq_state_off
#from skylink_wrapper.cython_skylink import auth_flag_auth_tx, auth_flag_require_auth, auth_flag_require_seq
import zmq, amqp
#import time
import json
from datetime import datetime as dtime
from queue import Queue, Empty
from urllib.parse import urlparse


DEBUG_PRINT_ON = True


def DBGPRINT(*args, **kwargs):
    ts = "[{}]".format( dtime.now().isoformat()[-15:] )
    ts += " "*(17-len(ts)) + "[SkyModem] " + " "
    first, args = args[0], args[1:]
    if DEBUG_PRINT_ON:
        print(ts+str(first), *args, **kwargs)


def sub_socket_loop(sub_sock:zmq.Socket, sub_que:Queue, vc_number, parent_obj):
    sub_sock.set(zmq.RCVTIMEO, 250)
    sub_sock.subscribe(b"")
    while parent_obj.on:
        try:
            rcv_msg = sub_sock.recv()
            sub_que.put_nowait((vc_number, rcv_msg))
        except zmq.Again:
            pass
        except Exception as e:
            DBGPRINT("Exception in zmq-subscribed socket loop: "+str(e))
            break


def bind_vc_sockets(vc_port_base, num_channels):
    context = zmq.Context()
    pub_sockets = list()
    sub_sockets = list()
    for i_vc in range(num_channels):
        pub_sock = context.socket(zmq.PUB)
        pub_sock.bind("tcp://*:{}".format( str(vc_port_base + i_vc*10) ))
        pub_sock.set(zmq.RCVTIMEO, 1000)
        pub_sockets.append(pub_sock)
        sub_sock = context.socket(zmq.SUB)
        sub_sock.bind(f"tcp://*:{str(vc_port_base + i_vc*10 + 1)}")
        sub_sock.subscribe(b"")
        sub_sock.set(zmq.RCVTIMEO, 1000)
        sub_sockets.append(sub_sock)
    signaldata_pub_sock = context.socket(zmq.PUB)
    signaldata_pub_sock.bind(f"tcp://*:{str(vc_port_base + 2)}")
    signaldata_pub_sock.set(zmq.RCVTIMEO, 1000)
    #signaldata_pub_sock.append(pub_sock)
    return pub_sockets, sub_sockets, signaldata_pub_sock, context


def connect_amqp_pub_socket(broker_addr):
    amqp_url = urlparse(broker_addr)
    amqp_conn = amqp.Connection(host=amqp_url.hostname, userid=amqp_url.username, password=amqp_url.password)
    amqp_conn.connect_timeout = 3
    amqp_conn.connect()
    ch = amqp_conn.channel()
    #ch.basic_publish(amqp.Message('Hello World'), routing_key='test')
    return ch, amqp_conn




class SkyModem:
    def __init__(self, rx_dsp_config:RXDSPConfig, tx_dsp_config:TXDSPConfig, radio_config:RadioConfig, skylink_config:SkyConfiguration, hmac_key_list, vc_port_base, amqp_broker_addr=None):
        self.on = True
        self.hmac_key_list = hmac_key_list
        self.que_samples_radio_to_dsp 	= Queue(2000)
        self.que_payloads_dsp_to_sky 	= Queue(100)
        self.que_payloads_sky_to_dsp 	= Queue(1)
        self.que_samples_dsp_to_radio 	= Queue(1)
        self.que_signaldata_out 		= Queue(100)
        self.radio_loop 	= RadioLoop(radio_config=radio_config, queue_tx_samples_from_dsp=self.que_samples_dsp_to_radio, queue_rx_samples_to_dsp=self.que_samples_radio_to_dsp)
        self.dsp_loop 		= DSPLoop(rx_dsp_config=rx_dsp_config, tx_dsp_config=tx_dsp_config, que_rx_samples_in=self.que_samples_radio_to_dsp, que_rx_payloads_out=self.que_payloads_dsp_to_sky,
                                                                   que_tx_payloads_in=self.que_payloads_sky_to_dsp, que_tx_samples_out=self.que_samples_dsp_to_radio, que_signaldata_out=self.que_signaldata_out)
        self.skylink_loop 	= SkyLinkLoop(config=skylink_config, key_list=hmac_key_list, que_payloads_in=self.que_payloads_dsp_to_sky, que_payloads_out=self.que_payloads_sky_to_dsp, radio_tx_sample_que=self.que_samples_dsp_to_radio, radio_ready_ev=self.radio_loop.tx_ready)
        self.sub_que_process_thread 	= threading.Thread(target=None, args=tuple())
        self.skylink_reception_thread 	= threading.Thread(target=None, args=tuple())
        pub_sockets, sub_sockets, signaldata_pub_sock, context = bind_vc_sockets(vc_port_base=vc_port_base, num_channels=num_virtual_channels)
        self.amqp_connection = None
        self.signaldata_amqp_pub_sock = None
        if not (amqp_broker_addr is None):
            try:
                self.signaldata_amqp_pub_sock, self.amqp_connection = connect_amqp_pub_socket(amqp_broker_addr)
            except:
                pass
        self.pub_sockets = pub_sockets
        self.sub_sockets = sub_sockets
        self.signaldata_pub_sock = signaldata_pub_sock
        self.zmq_ctx = context
        self.sub_threads = list()
        self.session_id_list = [arq_state_off,] * num_virtual_channels
        self.sub_que = Queue(128)
        self.action_lock = threading.RLock()


    def is_ok(self):
        if not self.on:
            return False
        if not self.sub_que_process_thread.is_alive():
            return False
        if not self.skylink_reception_thread.is_alive():
            return False
        for thrd in self.sub_threads:
            if not thrd.is_alive():
                return False
        if not self.skylink_loop.is_alive():
            return False
        if not self.radio_loop.is_ok():
            return False
        if not self.dsp_loop.is_ok():
            return False
        return True


    def close(self):
        self.on = False
        try:
            self.radio_loop.close()
        except:
            pass
        try:
            self.skylink_loop.close()
        except:
            pass
        try:
            self.dsp_loop.close()
        except:
            pass
        for thrd in (list(self.sub_threads) + [self.sub_que_process_thread, self.skylink_reception_thread]):
            try:
                thrd.join(timeout=1.0)
            except:
                pass


    def start(self, multimode=False):
        for i,sub_sock in enumerate(self.sub_sockets):
            vc_number = i
            thrd = threading.Thread(target=sub_socket_loop, args=(sub_sock, self.sub_que, vc_number, self), daemon=True)
            thrd.start()
            self.sub_threads.append(thrd)
        self.skylink_loop.start()
        if multimode:
            self.dsp_loop.start_multimode([9600,19200,38400], 0)
        else:
            self.dsp_loop.start()
        self.radio_loop.start()
        self.sub_que_process_thread 	= threading.Thread(target=self._zmq_to_modem_loop, args=tuple(), daemon=True)
        self.sub_que_process_thread.start()
        self.skylink_reception_thread 	= threading.Thread(target=self._modem_to_zmq_loop, args=tuple(), daemon=True)
        self.skylink_reception_thread.start()

    def sim_start(self, noiseSPD, ts_pl_list, rx_samplearr_que, tx_sample_que):
        self.skylink_loop.start()
        self.dsp_loop.start()
        self.radio_loop.sim_start(noiseSPD=noiseSPD, ts_pl_list=ts_pl_list, rx_samplearr_que=rx_samplearr_que, tx_sample_que=tx_sample_que)


    ## PRIVATE FUNCTIONS =====================================================================================================================================================================
    ## =======================================================================================================================================================================================
    def _modem_to_zmq_loop(self): # downlink
        while self.on:
            while not self.que_signaldata_out.empty():
                signaldata_tuple = self.que_signaldata_out.get_nowait()  # (t_unix, rx_f_absolute, power_tuple, self.dsp_config.baudrate, rx_pl)   |   power_tuple = (pl_power, noise_power, power_bw)
                signaldata_d = {
                        "t_unix": 			signaldata_tuple[0],
                        "rx_f_absolute": 	signaldata_tuple[1],
                        "pl_power": 		signaldata_tuple[2][0],
                        "noise_power": 		signaldata_tuple[2][1],
                        "power_bw": 		signaldata_tuple[2][2],
                        "baudrate": 		signaldata_tuple[3],
                        "pl": 				signaldata_tuple[4].hex(),
                }
                self.signaldata_pub_sock.send(json.dumps(signaldata_d).encode("utf8"))
                if self.signaldata_amqp_pub_sock:
                    self.signaldata_amqp_pub_sock.basic_publish(amqp.Message(json.dumps(signaldata_d)), routing_key="fs1p.store.signaldata", exchange="measurements")
            try:
                ekey, vc_number, rdata = self.skylink_loop.que_received_messages.get(timeout=0.20)
            except Empty:
                continue
            except Exception as e:
                DBGPRINT("Exception (skylink_reception_loop): "+str(e))
                self.close()
                break
            assert vc_number in range(num_virtual_channels)
            with self.action_lock:
                self._process_skylink_msg(ekey=ekey, vc_number=vc_number, data=rdata)


    def _process_skylink_msg(self, ekey, vc_number, data):
        if ekey in (EKEY_SKY_ARQ_CONNECTED, EKEY_SKY_ARQ_DISCONNECTED):
            session_id = data
            assert type(session_id) == int
            metadata_dict = dict()
            if ekey == EKEY_SKY_ARQ_CONNECTED:
                metadata_dict["rsp"] = "arq_connected"
            else:
                metadata_dict["rsp"] = "arq_timeout"
            metadata_dict["vc"] = vc_number
            metadata_dict["session_identifier"] = session_id
            response_dict = dict()
            response_dict["packet_type"] = "control"
            response_dict["vc"] = vc_number
            response_dict["timestamp"] = dtime.now().isoformat()
            response_dict["metadata"] = metadata_dict
            self.pub_sockets[vc_number].send(json.dumps(response_dict).encode("utf8"))
            self.session_id_list[vc_number] = session_id
        if ekey == EKEY_SKY_PAYLOAD:
            assert type(data) == bytes
            DBGPRINT(f"[VC: {vc_number} -> pub-zmq. len: {len(data)}]: \n\033[96m{data}\033[0m\n")
            frame_d = dict()
            frame_d["packet_type"] 	= "tm"
            frame_d["timestamp"] 	= dtime.now().isoformat()
            frame_d["vc"] 			= vc_number
            frame_d["data"] 		= "".join( [("00"+hex(x)[2:])[-2:] for x in data] )
            meta_d = dict()
            meta_d["vc"] 			= vc_number
            frame_d["metadata"] 	= meta_d
            self.pub_sockets[vc_number].send(json.dumps(frame_d).encode("utf8"))


    def _zmq_to_modem_loop(self): # uplink
        while self.on:
            try:
                vc_number, uplink_json = self.sub_que.get(timeout=0.20)
            except Empty:
                continue
            except Exception as e:
                DBGPRINT("Exception (sub_que_loop): "+str(e))
                self.close()
                break
            DBGPRINT("sub-zmq -> skylink-vc-{}. len: {}".format(vc_number, len(uplink_json)))
            with self.action_lock:
                self._process_uplink_json(vc_number, uplink_json)


    def _process_uplink_json(self, vc_number, uplink_json):
        if not vc_number in range(num_virtual_channels):
            DBGPRINT(f"error: ID not in vc range: {vc_number}")
            return
        frame_dict = json.loads( uplink_json )
        if not type(frame_dict) == dict:
            DBGPRINT(f"error: json was not a dict:{type(frame_dict)}")
            return

        if "data" in frame_dict:
            hex_string = frame_dict["data"]
            hex_pairs = [hex_string[i*2:i*2+2] for i in range(len(hex_string)//2)]
            ints = [int(x, 16) for x in hex_pairs]
            data = bytes(ints)
            send_ret = self.skylink_loop.send(vc_number=vc_number, data=data)
            if send_ret < 0:
                DBGPRINT("error: sky_vc_push_packet_to_send error: {}".format(send_ret))

        if "metadata" in frame_dict:
            response_dict = dict()
            control_dict = frame_dict["metadata"]
            if not "cmd" in control_dict:
                DBGPRINT("error: No 'cmd' field in control_dict")
                return
            ctrl_command = control_dict["cmd"]
            if ctrl_command == "get_state":
                list_of_vc_state_dicts 	= self.skylink_loop.sky_get_state()
                response_dict["rsp"] 	= "state"
                response_dict["state"] 	= list_of_vc_state_dicts
            elif ctrl_command == "flush":
                self.skylink_loop.flush(vc_number=vc_number)
                response_dict["rsp"] = "ack"
            elif ctrl_command == "get_stats":
                sky_stats_d = self.skylink_loop.sky_get_stats()
                response_dict["rsp"] = "stats"
                response_dict["skylink"] = sky_stats_d
                #response_dict["kuokka"] = dict()
            elif ctrl_command == "clear_stats":
                self.skylink_loop.sky_diag_clear()
            elif ctrl_command == "arq_connect":
                self.skylink_loop.arq_connect(vc_number=vc_number)
                response_dict["rsp"] = "arq_connecting"
                response_dict["session_identifier"] = self.skylink_loop.sky_get_state()[vc_number]["session_identifier"] #session_identifier
            elif ctrl_command == "arq_disconnect":
                self.skylink_loop.arq_disconnect(vc_number=vc_number)
                response_dict["rsp"] = "ack"
            elif ctrl_command == "set_baudrate":
                assert control_dict["baudrate"] in (4800, 9600, 9600*2, 9600*4), "invalid baudrate field in control_dict"
                self.dsp_loop.set_baudrate(control_dict["baudrate"])
                response_dict["rsp"] = "ack"
            elif ctrl_command == "set_skylink_config":
                r = self._set_skylink_config(control_dict["conf_idx"], control_dict["conf_value"])
                if r == 0:
                    response_dict["rsp"] = "ack"
            elif ctrl_command == "reset_skylink_config":
                r = self._reset_skylink_config()
                if r == 0:
                    response_dict["rsp"] = "ack"
            elif ctrl_command == "get_skylink_config":
                response_dict["skylink_config"] = self._get_skylink_config()
                response_dict["rsp"] = "skylink_config"
            else:
                DBGPRINT("Unknown control command: {}".format(ctrl_command))
                return

            rsp_frame_dict = dict()
            rsp_frame_dict["packet_type"] 	= "control"
            rsp_frame_dict["timestamp"] 	= dtime.now().isoformat()
            rsp_frame_dict["metadata"] 		= response_dict
            self.pub_sockets[vc_number].send(json.dumps(rsp_frame_dict).encode("utf8"))


    def _get_skylink_config(self):
        return self.skylink_loop.skylink.get_config_values()


    def _reset_skylink_config(self):
        new_config = SkyConfiguration(b"PyGS")
        new_skylink_loop = SkyLinkLoop(config=new_config,
                                                                        key_list=self.hmac_key_list,
                                                                        que_payloads_in=self.que_payloads_dsp_to_sky,
                                                                        que_payloads_out=self.que_payloads_sky_to_dsp,
                                                                        radio_tx_sample_que=self.que_samples_dsp_to_radio,
                                                                        radio_ready_ev=self.radio_loop.tx_ready
        )
        self.skylink_loop.close()
        self.skylink_loop = new_skylink_loop
        self.skylink_loop.start()
        return 0


    def _set_skylink_config(self, conf_idx:int, conf_value):
        self.skylink_loop.skylink.set_config_value(idx=conf_idx, value=conf_value)
        return 0
