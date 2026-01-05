import threading
import time
import numpy as np
import uhd
from queue import Queue, Empty
from dsp_library.kuokka.lib_reedsolomon import get_default_rs
from dsp_library.kuokka.lib_framing import frame_packet, encode_golay24
from dsp_library.kuokka.lib_tools import make_samples, FS1P_SYNCHWORD, FS1P_SYNCHWORD_LEN, ints_to_bits
import zmq
import struct
import json




def sub_socket_loop(sub_sock:zmq.Socket, sub_que:Queue, ichannel, parent_obj):
    sub_sock.set(zmq.RCVTIMEO, 250)
    sub_sock.subscribe(b"")
    while parent_obj.on:
        try:
            rcv_msg = sub_sock.recv()
            sub_que.put_nowait((ichannel, rcv_msg))
        except zmq.Again:
            pass
        except Exception as e:
            print("Exception in zmq-subscribed socket loop: "+str(e))
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
        sub_sock.bind("tcp://*:{}".format( str(vc_port_base + i_vc*10 + 1) ))
        sub_sock.subscribe(b"")
        sub_sock.set(zmq.RCVTIMEO, 1000)
        sub_sockets.append(sub_sock)

    return pub_sockets, sub_sockets, context





class UHFUsrpCharacterizer:
    def __init__(self, f_center_abs, baudrate, modulation_index, BT, usrp_tx_gain, zmq_port_base):
        rs_mx, rs_cfg 				= get_default_rs()
        self.preamble_bits 			= ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
        self.rs_mx					= rs_mx
        self.rs_cfg					= rs_cfg
        self.tx_gain				= usrp_tx_gain
        self.f_center_abs 			= f_center_abs
        self.f_tune_abs				= f_center_abs - (baudrate*1.2 + 200e3)
        self.sr0_tx 				= 1e6
        self.sr0_tx_actual 			= 1e6
        self.baudrate				= baudrate
        self.modulation_index				= modulation_index
        self.BT						= BT
        self.on 					= True
        self.que_tx_samples			= Queue(32)
        self.rx_thread 				= threading.Thread(target=None, args=tuple())
        self.tx_thread 				= threading.Thread(target=None, args=tuple())
        self.zmq_in_thread 			= threading.Thread(target=None, args=tuple())
        self.context = zmq.Context()
        pub_sock = self.context.socket(zmq.PUB)
        pub_sock.bind("tcp://127.0.0.1:{}".format( str(zmq_port_base) ))
        pub_sock.set(zmq.RCVTIMEO, 400)
        self.pub_sock = pub_sock
        sub_sock = self.context.socket(zmq.SUB)
        sub_sock.bind("tcp://127.0.0.1:{}".format( str(zmq_port_base + 1) ))
        sub_sock.subscribe(b"")
        sub_sock.set(zmq.RCVTIMEO, 400)
        self.sub_sock = sub_sock



    def is_ok(self):
        if not self.on:
            return False
        for thrd in (self.tx_thread, self.rx_thread, self.zmq_in_thread):
            if not thrd.is_alive():
                return False
        return True


    def close(self):
        self.on = False
        self.rx_thread.join(timeout=1.0)
        self.tx_thread.join(timeout=1.0)
        self.zmq_in_thread.join(timeout=1.0)


    def start(self):
        # This noise injection enforces the jit-compilation of much of the signal processing pipeline before the loop starts.
        print("USRP start")
        rx_gain = 40 # dB
        usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
        usrp.set_rx_rate(1e6, 0)
        usrp.set_tx_rate(self.sr0_tx, 0)
        usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(self.f_tune_abs), 0)
        usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(self.f_tune_abs), 0)
        usrp.set_rx_gain(rx_gain, 0)
        usrp.set_tx_gain(self.tx_gain, 0)
        self.sr0_tx_actual = float(usrp.get_tx_rate(0))
        print("RX gain range:      {}".format( str(usrp.get_rx_gain_range(0))[:-1] ))
        print("TX gain range:      {}".format( str(usrp.get_tx_gain_range(0))[:-1] ))
        print("usrp RX gain:       {}".format( usrp.get_rx_gain(0) ))
        print("usrp TX gain:       {}".format( usrp.get_tx_gain(0) ))
        print("usrp RX samplerate: {} ksps".format( round(usrp.get_rx_rate(0)*1e-3, 3) ))
        print("usrp TX samplerate: {} ksps".format( round(usrp.get_tx_rate(0)*1e-3, 3) ))
        print("usrp RX tune-f:     {} MHz".format( round(usrp.get_rx_freq(0)*1e-6, 3) ))
        print("usrp TX tune-f:     {} MHz".format( round(usrp.get_tx_freq(0)*1e-6, 3) ))
        self.rx_thread 			= threading.Thread(target=self._usrp_rx_loop,    args=(usrp,), daemon=True) #TODO bufferlen as setting?
        self.tx_thread 			= threading.Thread(target=self._usrp_tx_loop,    args=(usrp,), daemon=True) #TODO bufferlen as setting?
        self.zmq_in_thread 		= threading.Thread(target=self._zmq_in_loop,     args=tuple(), daemon=True) #TODO bufferlen as setting?
        self.on = True
        self.rx_thread.start()
        self.tx_thread.start()
        self.zmq_in_thread.start()


    def _compose_samples(self, payload, f_tx_abs, baudrate, modulation_index, BT, usrp_reshape, as_c64):
        f_offset_nrm = (f_tx_abs - self.f_tune_abs) / self.sr0_tx_actual
        f_baudrate_nrm = baudrate / self.sr0_tx_actual
        assert abs(f_offset_nrm) < (0.45 - f_baudrate_nrm*0.6)
        assert abs(f_offset_nrm) > (f_baudrate_nrm*0.6)
        pl_char_ints = np.array(bytearray(payload), dtype=np.int64)
        bits = frame_packet(pl=pl_char_ints, synchword_int=FS1P_SYNCHWORD, synchword_len=FS1P_SYNCHWORD_LEN, use_scrambler=False, use_rs=False, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg, nrz_shift=True)
        bits = np.concatenate( (self.preamble_bits, bits) )
        sps = self.sr0_tx_actual / baudrate
        n_silence_start = int(self.sr0_tx_actual * 2.0e-3)
        samples, _ = make_samples(samples_per_symbol=sps, bitstring=bits, frequency_offset=f_offset_nrm, power=1.0, modulation_index=modulation_index,
                                                   shaper_BT_prod=BT, n_silence_start=n_silence_start, n_silence_end=0)
        if usrp_reshape:
            samples = np.reshape(samples, (1, len(samples)))
        if as_c64:
            samples = np.array(samples, dtype=np.complex64)
        return samples



    def _usrp_rx_loop(self, usrp:uhd.usrp.MultiUSRP):
        # Set up the stream and receive buffer
        st_args = uhd.usrp.StreamArgs("fc32", "sc16")
        st_args.channels = [0]
        rx_streamer = usrp.get_rx_stream(st_args)
        # Start Stream
        stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
        stream_cmd.stream_now = True
        recv_buffer = np.zeros((1, 1024*4), dtype=np.complex64)
        metadata = uhd.types.RXMetadata()
        n_rx_loops = 0
        n_rx_total = 0
        avg_sr = 0.0
        t00 = time.perf_counter()
        rx_streamer.issue_stream_cmd(stream_cmd)
        while self.on:
            if (n_rx_loops % 5000) == 0:
                print("(rx-#{}) (sr~{} MS/s)".format(n_rx_loops, round(1e-6*avg_sr, 4) ))
            rx_ret = rx_streamer.recv(recv_buffer, metadata) #blocking until rx_buffer_len samples acquired
            avg_sr = n_rx_total / (time.perf_counter() - t00)
            n_rx_total += rx_ret
            n_rx_loops += 1


    def _usrp_tx_loop(self, usrp:uhd.usrp.MultiUSRP):
        tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
        # stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
        tx_stream_args.channels = [0]
        tx_streamer = usrp.get_tx_stream(tx_stream_args)
        tx_metadata = uhd.types.TXMetadata()
        tx_batch_len = 1024*2
        while self.on:
            try:
                samplearr = self.que_tx_samples.get(timeout=0.20)
            except Empty:
                continue
            except Exception as e:
                print("Queue.get() exception (tx-thread):", e)
                self.on = False
                break
            N = samplearr.shape[1]
            assert N > 100
            dtt = N / self.sr0_tx_actual
            idx = 0
            t_end = time.perf_counter() + dtt
            while idx < N:
                if (N - idx) <= tx_batch_len:
                    tx_metadata.end_of_burst = True
                tx_streamer.send(samplearr[0,idx:idx+tx_batch_len], tx_metadata)
                idx += tx_batch_len
            tx_metadata.end_of_burst = False
            t_to_end = max(0, t_end - time.perf_counter())
            time.sleep(t_to_end + 0.0e-3)
            #print("tx end. sleep of {}/{} ms.".format( round(t_to_end*1e3, 2), round(dtt*1e3, 2) ))



    def _zmq_in_loop(self):
        while self.on:
            try:
                rcv_msg = self.sub_sock.recv() # TODO: Add golay to packet
            except zmq.Again:
                continue
            except Exception as e:
                print("Exception in zmq-subscribed socket loop: "+str(e))
                break
            # Parse JSON and extract payload
            msg_dict = json.loads(rcv_msg.decode('utf-8'))
            pl = bytes.fromhex(msg_dict["data"])


            print("Transmitting {} bytes: {}".format(len(pl), pl))
            samples = self._compose_samples(payload=pl, f_tx_abs=self.f_center_abs, baudrate=self.baudrate, modulation_index=self.modulation_index, BT=self.BT, usrp_reshape=True, as_c64=True)
            self.que_tx_samples.put_nowait(samples)


# f_center_abs, baudrate, modulation_index, BT, usrp_tx_gain, zmq_port_base=7200

if __name__ == '__main__':
    import sys
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--f_center", type=float, required=True)
    parser.add_argument("--baudrate", type=int, required=True, choices=[9600, 9600*2, 9600*4])
    parser.add_argument("--modulation_index", type=float, required=True)
    parser.add_argument("--BT", type=float, required=True)
    args = parser.parse_args(sys.argv[1:])

    f_center = args.f_center
    baudrate = args.baudrate
    modulation_index = args.modulation_index
    BT = args.BT

    assert type(f_center) == float
    assert f_center > 400e6
    assert f_center < 500e6
    assert baudrate in (9600, 9600*2, 9600*4)
    assert 0.4 <= modulation_index <= 10.0
    assert (BT >= 0.5) or (BT == -1)
    print("Args: ", f_center, baudrate, modulation_index, BT)

    characterizer = UHFUsrpCharacterizer(f_center_abs=f_center, baudrate=baudrate, modulation_index=modulation_index, BT=BT, usrp_tx_gain=40, zmq_port_base=7200)
    characterizer.start()
    while True:
        time.sleep(1.0)
