//
// Created by elmore on 3.11.2021.
//

#include "zmq_trial.h"

#define ZMQ_URI_LEN 64




void zmq_trial(){
	void *context = zmq_ctx_new();
	void *publisher = zmq_socket(context, ZMQ_PUB);
	int rc = zmq_bind(publisher, "tcp://127.0.0.1:5556");
	assert(rc == 0);


	void *subscriber = zmq_socket(context, ZMQ_SUB);
	rc = zmq_connect(subscriber, "tcp://127.0.0.1:5556");
	assert(rc == 0);
	rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
	assert(rc == 0);
	sleepms(100);
	char message[12];
	PRINTFF(0,"Checkpint 1\n");

	while(1)
	{
		rc = zmq_send(publisher, "FOOBAR", 12, 0);
		assert(rc == 12);

		rc = zmq_recv(subscriber, message, 12, 0);
		assert(rc != -1);
		printf("%s\n", message);

		sleepms(100);
	}
	zmq_close(publisher);
	zmq_close(subscriber);
	zmq_ctx_destroy(context);
}



