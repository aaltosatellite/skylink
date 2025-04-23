import zmq, time, os


def main():
	context = zmq.Context()
	pub_sock = context.socket(zmq.PUB)
	pub_sock.connect("tcp://localhost:{}".format(str(7200)))
	pub_sock.set(zmq.RCVTIMEO, 1000)
	time.sleep(0.3)
	pub_sock.send(b"ABC-123" + os.urandom(32))


if __name__ == '__main__':
	main()