import socket
import sys
import time
import threading


def ping(ip, port):
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.settimeout(1.0)
	#s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	print("Connecting to: ", (ip,port))
	s.connect((ip, port))
	print("connected")
	s.send(b"ABC-123")
	time.sleep(0.2)
	#s.bind(("0.0.0.0", port))
	#s.listen(5)


def client_rcv(cli):
	cli.settimeout(2.0)
	rcv = cli.recv(2048)
	print("received:", rcv)


def serve(port):
	print("Serving at port", port)
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	s.bind(("0.0.0.0", port))
	s.listen(5)
	s.settimeout(2.0)
	while True:
		try:
			cs,addr = s.accept()
		except socket.timeout:
			continue
		print("Accepted incoming connection from", addr)
		cli_thrd = threading.Thread(target=client_rcv, args=(cs,))
		cli_thrd.start()
		#q = Queue(100)
		#rcv_process = mpr.Process(target=rcv_loop_tcp, args=(port, q))
		#rcv_process.start()
		#print("Rcv loop started.")
		#cs,addr = s.accept()



if __name__ == "__main__":
	args = sys.argv[1:]
	assert args[0] in ("ping", "serve")
	if args[0] == "ping":
		assert len(args) == 3
		ip = args[1]
		port = int(args[2])
		ping(ip, port)
	else:
		assert len(args) == 2
		port = int(args[1])
		serve(port)



