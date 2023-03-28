
#include "vc_interface.hpp"

#include <iostream>
#include <string>

#include <json.hpp> // nlohmann JSON library stolen from suo library
#include <frame-io/zmq_interface.hpp> // To use same zmq_ctx as suo


using namespace std;
using namespace suo;
using json = nlohmann::json;

#define ZMQ_URI_LEN 64
#define PACKET_MAXLEN 256


VCInterface::VCInterface(SkyHandle protocol_handle, unsigned int vc_base)
{
	char uri[ZMQ_URI_LEN];
	for (unsigned int vc_index = 0; vc_index < SKY_NUM_VIRTUAL_CHANNELS; vc_index++) {

		VirtualChannelInterface &vc = vcs[vc_index];
		vc.vc_index = vc_index;
		vc.vc_handle = protocol_handle->virtual_channels[vc_index];
		vc.protocol_handle = protocol_handle;

		// Create ZMQ publish sockets to data received by Skylink
		snprintf(uri, ZMQ_URI_LEN, "tcp://*:%u", vc_base + 10 * vc_index);
		SKY_PRINTF(SKY_DIAG_INFO, "VC %d RX binding %s\n", vc_index, uri);
		vc.publish_socket = zmq::socket_t(zmq_ctx, zmq::socket_type::pub);
		vc.publish_socket.bind(uri);

		// Create ZMQ subscribe sockets to
		snprintf(uri, ZMQ_URI_LEN, "tcp://*:%u", vc_base + 10 * vc_index + 1);
		SKY_PRINTF(SKY_DIAG_INFO, "VC %d TX binding %s\n", vc_index, uri);
		vc.subscribe_socket = zmq::socket_t(zmq_ctx, zmq::socket_type::sub);
		vc.subscribe_socket.bind(uri);
		vc.subscribe_socket.set(zmq::sockopt::subscribe, "");
	}
	flush();
}


void VCInterface::flush() 
{
	for (VirtualChannelInterface &vc : vcs)
	{
		zmq::message_t msg;
		vc.subscribe_socket.set(zmq::sockopt::rcvtimeo, 250); // [ms]
		while (1)
		{
			auto res = vc.subscribe_socket.recv(msg);
			if (res.has_value() == false)
				break;
		}
	}
}


void VCInterface::tick(Timestamp now)
{
	(void)now;
	for (VirtualChannelInterface& vc: vcs) {
		try { 
			vc.check();
		}
		catch (const SuoError& e) {
			cerr << "VC" << vc.vc_index << " failed! " << e.what() << endl;
		}
	}
}

void VCInterface::VirtualChannelInterface::check()
{
	/* 
	 * Has ARQ changed the state?
	 */
	if (arq_expected_state != ARQ_STATE_OFF) {
		// Has ARQ turned off
		if (vc_handle->arq_state_flag == ARQ_STATE_OFF) {
			arq_expected_state = ARQ_STATE_OFF;

			SKY_PRINTF(SKY_DIAG_ARQ, "VC%d ARQ has disconnected!\n", vc_index);

			/* Send ARQ disconnected message */
			json metadata_dict = json::object();
			metadata_dict["rsp"] = "arq_timeout";
			metadata_dict["vc"] = vc_index;

			json response_dict = json::object();
			response_dict["type"] = "control";
			response_dict["timestamp"] = getISOCurrentTimestamp();
			response_dict["metadata"] = metadata_dict;

			// Serialize dict and send it socket
			string frame_str(response_dict.dump());
			publish_socket.send(zmq::buffer(frame_str), zmq::send_flags::dontwait);
		}
	}
	else {
		/* Has ARQ turned on? */
		if (vc_handle->arq_state_flag != ARQ_STATE_OFF) {
			arq_expected_state = ARQ_STATE_ON;
				
#if 0
			// TODO: Send when state has changed on! now this triggers when the handshake (ARQ_STATE_IN_INIT) is being transmitted.

			/* Send ARQ disconnected message */
			json metadata_dict = json::object();
			metadata_dict["rsp"] = "arq_connected";
			metadata_dict["vc"] = index;

			json response_dict = json::object();
			response_dict["type"] = "control";
			response_dict["vc"] = index;
			response_dict["timestamp"] = getISOCurrentTimestamp();
			response_dict["metadata"] = metadata_dict;

			// Serialize dict and send it socket
			string frame_str(response_dict.dump());
			publish_socket.send(zmq::buffer(frame_str), zmq::send_flags::dontwait);
#endif
		}
	}

	/*
	 * If packets appeared to some RX buffer, send them to ZMQ
	 */
	uint8_t data[PACKET_MAXLEN];
	int ret = sky_vc_read_next_received(vc_handle, data + 1, PACKET_MAXLEN);
	if (ret > 0) {
		const size_t data_len = (size_t)ret;

		SKY_PRINTF(SKY_DIAG_INFO | SKY_DIAG_FRAMES, "VC%u: Received %lu bytes\n", vc_index, data_len);

		// Output the frame in porthouse's (frame format?)
		json frame_dict = json::object();
		frame_dict["packet_type"] = "downlink";
		frame_dict["timestamp"] = getISOCurrentTimestamp();
		frame_dict["vc"] = vc_index;

		// Format binary data to hexadecimal string
		stringstream hexa_stream;
		hexa_stream << setfill('0') << hex;
		for (size_t i = 0; i < data_len; i++)
			hexa_stream << setw(2) << (int)data[i];
		frame_dict["data"] = hexa_stream.str();

		// Metadata
		json meta_dict = json::object();
		meta_dict["vc"] = vc_index;
		// Something more?
		frame_dict["metadata"] = meta_dict;

		// Serialize dict and send it socket
		string frame_str(frame_dict.dump());
		publish_socket.send(zmq::buffer(frame_str), zmq::send_flags::dontwait);
	}


	/*
	 * Try to receive a message from subcriber socket.
	 */
	zmq::message_t msg;
	try {
		auto res = subscribe_socket.recv(msg, zmq::recv_flags::dontwait);
		if (res.has_value() == false)
			return;
	}
	catch (const zmq::error_t& e) {
		cerr << "zmq_recv_frame: Failed to read data. " << e.what() << endl;
	}

	// Try to parse received message as JSON dictionary 
	json frame_dict;
	try {
		// TODO: Check that the packet meets basic Skylink JSON requirements??
		frame_dict = json::parse(msg.to_string());
		if (frame_dict.is_object() == false)
			throw SuoError("Received JSON string was not a dict/object. %s", msg.to_string());
	}
	catch (const json::exception& e) {
		throw SuoError("Failed to parse received ZMQ message", e.what());
	}

	// If the frame has some data, pass it to Skylink 
	if (frame_dict.contains("data") && frame_dict["data"].is_null() == false)
	{
		// Check packet type
		//string packet_type = frame_dict.value("packet_type", "None");
		//if (packet_type != "tc")
		//	cerr << "TODO: handle unknown packet type -> " << packet_type << endl;

		// Parse ASCII hexadecimal string to bytes
		string hex_string = frame_dict["data"];
		if (hex_string.size() % 2 != 0)
			throw SuoError("JSON data field has odd number of characters!");

		unsigned int tx_vc = frame_dict.value("vc", vc_index);
		if (tx_vc >= SKY_NUM_VIRTUAL_CHANNELS) 
			throw SuoError("Invalid virtual channel index!", tx_vc);

		size_t frame_len = hex_string.size() / 2;
		ByteVector data(frame_len);
		for (size_t i = 0; i < frame_len; i++)
			data[i] = stoul(hex_string.substr(2 * i, 2), nullptr, 16);

		// Write data to skylink buffer
		SkyVirtualChannel * tx_vc_handle = protocol_handle->virtual_channels[tx_vc];
		SKY_PRINTF(SKY_DIAG_INFO | SKY_DIAG_FRAMES, "VC%u: Sending %lu bytes\n", tx_vc, data.size());
		int ret = sky_vc_push_packet_to_send(tx_vc_handle, &data[0], data.size());
		if (ret < 0)
			SKY_PRINTF(SKY_DIAG_BUG, "VC%u: Failed to push new frame! %u\n", tx_vc, ret);
	}
	else {

		SKY_PRINTF(SKY_DIAG_DEBUG, "CTRL MSG vc: %u len: msg_len %lu\n", vc_index, msg.size());

		// Received frame didn't contain any data so try to read the metadata
		json control_dict = frame_dict["metadata"];
		if (control_dict.is_object() == false)
			throw SuoError("asaa (control_dict is not an object)");


		json response_dict = json::object();
		string ctrl_command = control_dict.value("cmd", "");

		if (ctrl_command == "get_state") {
			/*
			 * Get virtual channel buffer status
			 */

			SkyState state;
			sky_get_state(protocol_handle, &state);

			// Dump the state struct to JSON
			json vc_list = json::array();
			for (size_t vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++)
			{
				json vc_dict = json::object();
				vc_dict["arq_state"] = (unsigned int)state.vc[vc].state;
				vc_dict["buffer_free"] = (unsigned int)state.vc[vc].free_tx_slots;
				vc_dict["tx_frames"] = (unsigned int)state.vc[vc].tx_frames;
				vc_dict["rx_frames"] = (unsigned int)state.vc[vc].rx_frames;
				vc_list.push_back(vc_dict);
			}

			response_dict["rsp"] = "state";
			response_dict["state"] = vc_list;
		}
		else if (ctrl_command == "flush")
		{
			/*
			 * Flush virtual channel buffers
			 */
			sky_vc_wipe_to_arq_off_state(vc_handle);
			// No response
		}
		else if (ctrl_command == "get_stats")
		{
			/*
			 * Get statistics
			 */

			// Dump skylink stats
			json sky_stats_dict = json::object();
			SkyDiagnostics stats = *protocol_handle->diag;
			sky_stats_dict["rx_frames"] = stats.rx_frames;
			sky_stats_dict["rx_arq_resets"] = stats.rx_arq_resets;
			sky_stats_dict["tx_frames"] = stats.tx_frames;
			sky_stats_dict["tx_bytes"] = stats.tx_bytes;

			// Dump suo statistics
			json suo_stats_dict = json::object();
			// TODO: Implement stats dump for suo

			response_dict["rsp"] = "stats";
			response_dict["skylink"] = sky_stats_dict;
			response_dict["suo"] = suo_stats_dict;
		}
		else if (ctrl_command == "clear_stats")
		{
			/*
			 * Reset statistics
			 */
			SKY_PRINTF(SKY_DIAG_INFO, "Statistics cleared\n");
			memset(protocol_handle->diag, 0, sizeof(SkyDiagnostics));
			// No response
		}
		else if (ctrl_command == "set_config")
		{
			/*
			 * Set Skylink Configuration
			 */

			string config_name = control_dict["config"];
			string config_value = control_dict["value"];
			set_config(config_name, config_value);
			
			// No response
		}
		else if (ctrl_command == "get_config")
		{
			/*
			 * Get Skylink Configuration
			 */
			string config_name = control_dict["config"];

			// TODO: Give correct config value for requested config
			json config_value = 3;

			response_dict["rsp"] = "config";
			response_dict["config"] = config_name;
			response_dict["value"] = config_value;
		}
		else if (ctrl_command == "arq_connect")
		{
			/*
			 * ARQ connect
			 */
			SKY_PRINTF(SKY_DIAG_ARQ, "VC%u ARQ connecting\n", vc_index);
			sky_vc_wipe_to_arq_init_state(vc_handle);
			// No response
		}
		else if (ctrl_command == "arq_connect")	// TODO: Fix typo?? could be arq_disconnect
		{
			/*
			 * ARQ disconnect
			 */
			SKY_PRINTF(SKY_DIAG_ARQ, "VC%u ARQ disconnecting\n", vc_index);
			sky_vc_wipe_to_arq_off_state(vc_handle);
			// No response
		}
		else if (ctrl_command == "mac_reset")
		{
			/*
			 * MAC/TDD Reset
			 */
			SKY_PRINTF(SKY_DIAG_MAC, "Commanded MAC reset\n");
			mac_reset(protocol_handle->mac, sky_get_tick_time());
			// No response
		}
		else {
			cerr << "Unknown ZMQ message '" << msg.to_string() << "'" << endl;
		}

		// Send response back to publisher socket
		if (response_dict.empty() == false) {

			json frame_dict = json::object();
			frame_dict["type"] = "control";
			frame_dict["timestamp"] = getISOCurrentTimestamp();
			frame_dict["metadata"] = response_dict;

			// Serialize dict and send it socket
			string frame_str(frame_dict.dump());
			publish_socket.send(zmq::buffer(frame_str), zmq::send_flags::dontwait);
		}
	}
}


#if 0

template <typename Member>
static void set(SkyHandle handle, Member member, const stringstream& value)
{
	cout << typeid(member).name() << endl;
	if (parameter == typeid(member).name())
		value >> handle.*member;
}

void set_config(const string &parameter, const string &value_)
{
	stringstream value(value_);

	set(protocol_handle, mac.maximum_window_length_ticks, value);
	set(protocol_handle, mac.minimum_window_length_ticks, value);
	set(protocol_handle, mac.gap_constant_ticks, value);
	set(protocol_handle, mac.tail_constant_ticks, value);
	set(protocol_handle, mac.shift_threshold_ticks, value);
	set(protocol_handle, mac.idle_timeout_ticks, value);
	set(protocol_handle, mac.window_adjust_increment_ticks, value);
	set(protocol_handle, mac.carrier_sense_ticks, value);
	set(protocol_handle, mac.unauthenticated_mac_updates, value);
	set(protocol_handle, mac.window_adjustment_period, value);
	set(protocol_handle, mac.idle_frames_per_window, value);

	set(protocol_handle, arq_timeout_ticks, value);
	set(protocol_handle, arq_idle_frame_threshold, value);
	set(protocol_handle, arq_idle_frames_per_window, value);

}

#else
void VCInterface::VirtualChannelInterface::set_config(const string &parameter, const string &value)
{
#define CONFIG_I(param_name)                                      \
	if (parameter == #param_name)                                 \
	{                                                             \
		protocol_handle->conf->param_name = atoll(value.c_str()); \
		return;                                                   \
	}

	CONFIG_I(mac.maximum_window_length_ticks);
	CONFIG_I(mac.minimum_window_length_ticks);
	CONFIG_I(mac.gap_constant_ticks);
	CONFIG_I(mac.tail_constant_ticks);
	CONFIG_I(mac.shift_threshold_ticks);
	CONFIG_I(mac.idle_timeout_ticks);
	CONFIG_I(mac.window_adjust_increment_ticks);
	CONFIG_I(mac.carrier_sense_ticks);
	CONFIG_I(mac.unauthenticated_mac_updates);
	CONFIG_I(mac.window_adjustment_period);
	CONFIG_I(mac.idle_frames_per_window);

	CONFIG_I(arq_timeout_ticks);
	CONFIG_I(arq_idle_frame_threshold);
	CONFIG_I(arq_idle_frames_per_window);

#undef CONFIG_I

	cout << "No such Skylink configuration: " << parameter << endl;
}
#endif
