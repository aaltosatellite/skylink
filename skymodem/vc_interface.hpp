#pragma once

#include <array>
#include <zmq.hpp>

#include "suo.hpp"
#include "skylink.hpp"


class VCInterface {
public:
	
	struct VirtualChannelInterface
	{
		unsigned int vc_index;
		zmq::socket_t publish_socket, subscribe_socket;
		int arq_expected_state;

		// Skylink pointers
		SkyHandle protocol_handle;
		SkyVirtualChannel *vc_handle;

		void check();
		/* Set Skylink configuration */
		void set_config(const std::string &parameter, const std::string &value);
	};

	/* */
	VCInterface(SkyHandle protocol_handle, unsigned int base);

	/* Flush possible queued frames */
	void flush();

	/* */
	void tick(suo::Timestamp now);


private:
	std::array<VirtualChannelInterface, SKY_NUM_VIRTUAL_CHANNELS> vcs;
};
