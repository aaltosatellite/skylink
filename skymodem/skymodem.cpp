#include <string>
#include <iostream>
#include <fstream>
#include <cstring> // memcpy

#include "skymodem.hpp"

using namespace std;
using namespace suo;


constexpr sky_tick_t convert_to_ticks(Timestamp now) {
	return now / 1000000; // Convert nanoseconds to milliseconds
}


SkyModem::SkyModem() :
	sdr(nullptr),
	zmq_output(nullptr),
	vc_interface(nullptr)
{

	/* -------------------------
	 * Initialize protocol
	 * ------------------------- */

	sky_diag_mask = SKY_DIAG_INFO | SKY_DIAG_FRAMES | SKY_DIAG_ARQ | SKY_DIAG_BUG;

	/*
	 * PHY configurations
	 */
	config.phy.enable_scrambler = 0;
	config.phy.enable_rs = 0;

	SKY_ASSERT(SKY_IDENTITY_LEN == 6);
	config.identity[0] = 'D';
	config.identity[1] = 'e';
	config.identity[2] = 'f';
	config.identity[3] = 'a';
	config.identity[4] = 'u';
	config.identity[5] = 'l';
	//config.identity[6] = 't';

	/*
	 * MAC configurations
	 */
	config.mac.gap_constant_ticks = 350;
	config.mac.tail_constant_ticks = 60;
	config.mac.maximum_window_length_ticks = 5000;
	config.mac.minimum_window_length_ticks = 250;
	config.mac.window_adjust_increment_ticks = 250;
	config.mac.window_adjustment_period = 2;
	config.mac.unauthenticated_mac_updates = 0;
	config.mac.shift_threshold_ticks = 10000;
	config.mac.idle_frames_per_window = 1;
	config.mac.idle_timeout_ticks = 10000;
	config.mac.carrier_sense_ticks = 200;

	/*
	 * Virtual channel configurations
	 */
	config.vc[0].horizon_width = 16;
	config.vc[0].send_ring_len = 24;
	config.vc[0].rcv_ring_len = 24;
	config.vc[0].element_size = 36;
	config.vc[0].require_authentication = SKY_VC_FLAG_REQUIRE_AUTHENTICATION | SKY_VC_FLAG_AUTHENTICATE_TX;

	config.vc[1].horizon_width = 16;
	config.vc[1].send_ring_len = 24;
	config.vc[1].rcv_ring_len = 24;
	config.vc[1].element_size = 36;
	config.vc[1].require_authentication = SKY_VC_FLAG_REQUIRE_AUTHENTICATION | SKY_VC_FLAG_AUTHENTICATE_TX;

	config.vc[2].horizon_width = 2;
	config.vc[2].send_ring_len = 8;
	config.vc[2].rcv_ring_len = 8;
	config.vc[2].element_size = 36;
	config.vc[2].require_authentication = SKY_VC_FLAG_REQUIRE_AUTHENTICATION | SKY_VC_FLAG_AUTHENTICATE_TX;

	config.vc[3].horizon_width = 2;
	config.vc[3].send_ring_len = 8;
	config.vc[3].rcv_ring_len = 8;
	config.vc[3].element_size = 36;
	config.vc[3].require_authentication = 0;


	config.arq_timeout_ticks = 12000; // [ticks]
	config.arq_idle_frame_threshold = config.arq_timeout_ticks / 4; // [ticks]
	config.arq_idle_frames_per_window = 1;


	/*
	 * HMAC configuration
	 */
#ifdef EXTERNAL_SECRET
#include "secret.hpp"
#else
	const uint8_t hmac_key[32] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69};
	const unsigned int hmac_key_len = sizeof(hmac_key);
#endif
	config.hmac.key_length = hmac_key_len;
	config.hmac.maximum_jump = 24;
	assert(hmac_key_len <= sizeof(config.hmac.key));
	memcpy(config.hmac.key, hmac_key, hmac_key_len);

	// Kick the actual protocol implementation running
	handle = new sky_all;
	handle->conf = &config;
	handle->mac = sky_mac_create(&config.mac);
	handle->hmac = sky_hmac_create(&config.hmac);
	handle->diag = sky_diag_create();
	
	for (unsigned int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i)
	{
		handle->virtual_channels[i] = new_virtual_channel(&config.vc[i]);
		if (handle->virtual_channels[i] == NULL)
		{
			cerr << "Failed to create virtual channel %d" << endl;
			return;
		}
	}


	/* 
	 * Initialize
	 */
	vc_interface = new VCInterface(handle, 7100);

	try {

		const float center_frequency = 437.125e6; // [Hz]

		/*
		 * SDR
		 */
		SoapySDRIO::Config sdr_conf;
		sdr_conf.rx_on = true;
		sdr_conf.tx_on = true;
		sdr_conf.half_duplex = true;

		sdr_conf.idle_delay = 10e6; // [ns] 
		sdr_conf.transmission_delay = 10e6; // [ns]

		sdr_conf.use_time = 1;
		sdr_conf.samplerate = 500000; // [Hz]
		sdr_conf.tx_latency = 0; // [samples]
		sdr_conf.buffer = 2 * (sdr_conf.samplerate / 1000); // buffer length in milliseconds

		sdr_conf.args["driver"] = "uhd";

		sdr_conf.rx_centerfreq = 437.00e6;
		sdr_conf.tx_centerfreq = sdr_conf.rx_centerfreq;

		sdr_conf.rx_gain = 30;
		sdr_conf.tx_gain = 60;

		sdr_conf.rx_antenna = "TX/RX";
		sdr_conf.tx_antenna = "TX/RX";

		sdr = new SoapySDRIO(sdr_conf);
		sdr->sinkTicks.connect_member(this, &SkyModem::tick);
		sdr->sinkTicks.connect_member(vc_interface, &VCInterface::tick);

		/*
		 * Setup receiver
		 */
		GMSKContinousDemodulator::Config receiver_conf;
		receiver_conf.sample_rate = sdr_conf.samplerate;
		receiver_conf.center_frequency = center_frequency - sdr_conf.rx_centerfreq;

		/*
		 * Setup frame decoder
		 */
		GolayDeframer::Config deframer_conf;
		deframer_conf.syncword = 0x1ACFFC1D;
		deframer_conf.syncword_len = 32; // [symbols/bits]
		deframer_conf.sync_threshold = 3;
		deframer_conf.use_viterbi = false;
		deframer_conf.use_randomizer = true;
		deframer_conf.use_rs = true;


		/* For 9600 baud */
		receiver_conf.symbol_rate = 9600;
		receiver_9k6 = new GMSKContinousDemodulator(receiver_conf);
		deframer_9k6 = new GolayDeframer(deframer_conf);
		receiver_9k6->sinkSymbol.connect_member(deframer_9k6, &GolayDeframer::sinkSymbol);
		receiver_9k6->setMetadata.connect_member(deframer_9k6, &GolayDeframer::setMetadata);

		deframer_9k6->syncDetected.connect( [&] (bool locked, Timestamp now) {
			receiver_9k6->lockReceiver(locked, now);
		});

		sdr->sinkSamples.connect_member(receiver_9k6, &GMSKContinousDemodulator::sinkSamples);

#ifdef MULTIMODE
		/* For 19200 baud */
		receiver_conf.symbol_rate = 19200;
		receiver_19k2 = new GMSKContinousDemodulator(receiver_conf);
		deframer_19k2 = new GolayDeframer(deframer_conf);
		receiver_19k2->sinkSymbol.connect_member(deframer_19k2, &GolayDeframer::sinkSymbol);
		receiver_19k2->setMetadata.connect_member(deframer_19k2, &GolayDeframer::setMetadata);
		sdr->sinkSamples.connect_member(receiver_19k2, &FSKMatchedFilterDemodulator::sinkSamples);

		/* For 36400 baud */
		receiver_conf.symbol_rate = 36400;
		receiver_36k4 = new GMSKContinousDemodulator(receiver_conf);
		deframer_36k4 = new GolayDeframer(deframer_conf);
		receiver_36k4->sinkSymbol.connect_member(deframer_36k4, &GolayDeframer::sinkSymbol);
		receiver_36k4->setMetadata.connect_member(deframer_36k4, &GolayDeframer::setMetadata);
		sdr->sinkSamples.connect_member(receiver_36k4, &FSKMatchedFilterDemodulator::sinkSamples);

		deframer_9k6->syncDetected.connect( [&](bool locked, Timestamp now) {
			cout << getISOCurrentTimestamp() << ": 9600 sync detected!" << endl;
			receiver_locked(locked, now);
			receiver_9k6->lockReceiver(locked, now);
		});
		deframer_9k6->sinkFrame.connect( [&] (Frame& frame, Timestamp now) {
			frame.setMetadata("mode", 9600);
			frame_received(locked, now);
		});

		deframer_19k2->syncDetected.connect( [&](bool locked, Timestamp now) {
			cout << getISOCurrentTimestamp() << ": 19200 sync detected!" << endl;
			receiver_locked(locked, now);
			receiver_19k2->lockReceiver(locked, now);
		});
		deframer_19k2->sinkFrame.connect( [&] (Frame& frame, Timestamp now) {
			frame.setMetadata("mode", 19200);
			frame_received(locked, now);
		});

		deframer_36k4->syncDetected.connect( [&] (bool locked, Timestamp now) {
			cout << getISOCurrentTimestamp() << ": 36400 sync detected!" << endl;
			receiver_locked(locked, now);
			receiver_36k4->lockReceiver(locked, now);
		});
		deframer_36k4->sinkFrame.connect( [&] (Frame& frame, Timestamp now) {
			frame.setMetadata("mode", 364600);
			frame_received(locked, now);
		});

#else
		deframer_9k6->syncDetected.connect( [&] (bool locked, Timestamp now) {
			receiver_locked(locked, now);
			receiver_9k6->lockReceiver(locked, now);
		});
		deframer_9k6->sinkFrame.connect( [&] (Frame& frame, Timestamp now) {
			frame.setMetadata("mode", 9600);
			frame_received(frame, now);
		});

#endif


		/*
		 * ZMQ output for raw frames
		 */
		ZMQPublisher::Config zmq_output_conf;
		zmq_output_conf.bind = "tcp://127.0.0.1:7200";
		zmq_output_conf.msg_format = ZMQMessageFormat::JSON;

		zmq_output = new ZMQPublisher(zmq_output_conf);


		/*
		 * Setup transmitter
		 */
		GMSKModulator::Config modulator_conf;
		modulator_conf.sample_rate = sdr_conf.samplerate;
		modulator_conf.symbol_rate = 9600;
		modulator_conf.center_frequency = center_frequency - sdr_conf.tx_centerfreq;
		modulator_conf.bt = 0.5;
		modulator_conf.ramp_up_duration = 2;
		modulator_conf.ramp_down_duration = 2;

		modulator = new GMSKModulator(modulator_conf);

		/*
		 * Setup framer
		 */
		GolayFramer::Config framer_conf;
		framer_conf.syncword = deframer_conf.syncword;
		framer_conf.syncword_len = deframer_conf.syncword_len;
		framer_conf.preamble_len = 12 * 8; // [symbols/bits]
		framer_conf.use_viterbi = false;
		framer_conf.use_randomizer = true;
		framer_conf.use_rs = true;

		framer = new GolayFramer(framer_conf);
		framer->sourceFrame.connect_member(this, &SkyModem::frame_transmit);
		modulator->generateSymbols.connect_member(framer, &GolayFramer::generateSymbols);
		sdr->generateSamples.connect_member(modulator, &GMSKModulator::generateSamples);


#ifdef USE_PORTHOUSE_TRACKER
		/*
		 * Setup porthouse tracker
		 */
		PorthouseTracker::Config tracker_conf;
		tracker_conf.amqp_url = "amqp://guest:guest@localhost/";
		tracker_conf.target_name = "Foresail-1";
		tracker_conf.center_frequency = center_frequency;

		tracker = new PorthouseTracker(tracker_conf);
		sdr->sinkTicks.connect_member(tracker, &PorthouseTracker::tick);

#ifdef MULTIMODE
		tracker->setDownlinkFrequency.connect( [ center_frequency, this ] (float frequency) {
			receiver_9k6->setFrequencyOffset(frequency - center_frequency);
			receiver_19k2->setFrequencyOffset(frequency - center_frequency);
			receiver_36k4->setFrequencyOffset(frequency - center_frequency);
		});
#else
		tracker->setDownlinkFrequency.connect( [ center_frequency, this ] (float frequency) {
			receiver_9k6->setFrequencyOffset(frequency - center_frequency);
		});
#endif
		tracker->setUplinkFrequency.connect([ center_frequency, this ] (float frequency) {
			modulator->setFrequencyOffset(frequency - center_frequency);
		});
#endif
		
	}
	catch (const SuoError& e)
	{
		cerr << "SuoError: " << e.what() << endl;
	}
	catch (const std::exception &e)
	{
		cerr << "std::exception: " << e.what() << endl;
	}
}


SkyModem::~SkyModem()
{
	if (sdr != nullptr)
		delete sdr;
	if (zmq_output != nullptr)
		delete zmq_output;
	if (vc_interface != nullptr)
		delete vc_interface;
#ifdef USE_PORTHOUSE_TRACKER
	if (tracker != nullptr)
		delete tracker;
#endif
}


int SkyModem::run()
{
	try {
		sdr->execute();
		cerr << "Suo exited" << endl;
		return 0;
	}
	catch (const SuoError &e)
	{
		cerr << "SuoError: " << e.what() << endl;
	}
	catch (const std::exception &e)
	{
		cerr << "std::exception: " << e.what() << endl;
	}
	return 1;
}

void SkyModem::tick(Timestamp now)
{
	sky_tick(convert_to_ticks(now));
}


void SkyModem::receiver_locked(bool locked, Timestamp now)
{
	//sdr->lock_tx(locked);

	if (locked) {
		sky_mac_carrier_sensed(handle->mac, convert_to_ticks(now));
		//cout << getISOCurrentTimestamp() << ": Sync detected: " << (locked ? "true": "false") << endl;
	}
	else {

	}
}

void SkyModem::frame_transmit(Frame &frame, Timestamp now)
{
	(void)now;

	// Request a frame to be transmitted from the Skylink implementation 
	SkyRadioFrame sky_frame;
	int ret = sky_tx(handle, &sky_frame);
	if (ret < 0)
		SKY_PRINTF(SKY_DIAG_BUG, "sky_tx() error %d\n", ret);

	if (ret == 1) { // A new physical frame was produced. 
		
		// Copy data from Skylink's frame structure to suo's frame structure
		frame.data.resize(sky_frame.length);
		memcpy(&frame.data[0], sky_frame.raw, sky_frame.length);

		// Pass the raw frame also to ZMQ sink
		frame.setMetadata("mode", 9600);
		frame.setMetadata("packet_type", "uplink_raw");
		frame.setMetadata("utc_timestamp", getISOCurrentTimestamp());

		cout << getISOCurrentTimestamp() << ": Frame transmit";
		cout << frame(Frame::PrintData | Frame::PrintMetadata | Frame::PrintColored | Frame::PrintAltColor);

		zmq_output->sinkFrame(frame, now);
		// TODO: Insert somethow ISO timestamp to the frame. SDR timestamp is not needed.
	}
}

void SkyModem::frame_received(Frame &frame, Timestamp now)
{
	cout << getISOCurrentTimestamp() << ": Frame received ";
	cout << frame(Frame::PrintData | Frame::PrintMetadata | Frame::PrintColored);

	// Copy data from suo's frame structure to Skylink's frame structure
	SkyRadioFrame sky_frame;
	sky_frame.rx_time_ticks = convert_to_ticks(frame.timestamp);
	sky_frame.length = frame.size();
	memcpy(sky_frame.raw, &frame.data[0], frame.size());

	// Pass the frame to Skylink implementation
	int ret = sky_rx(handle, &sky_frame);
	if (ret < 0)
		SKY_PRINTF(SKY_DIAG_BUG, "sky_rx() error %d\n", ret);

	// Pass the raw frame also to ZMQ sink
	frame.setMetadata("packet_type", "downlink_raw");
	frame.setMetadata("utc_timestamp", getISOCurrentTimestamp());
	zmq_output->sinkFrame(frame, now);
}

void SkyModem::load_sequence_numbers()
{
	ifstream sequence_file("sequences");
	if (sequence_file) {
		cerr << "Failed to open sequence file for reading" << endl;
		return;
	}

	int32_t sequences[2 * SKY_NUM_VIRTUAL_CHANNELS] = {0};
	for (int i = 0; i < 2 * SKY_NUM_VIRTUAL_CHANNELS; i++)
		sequence_file >> sequences[i];
	sky_hmac_load_sequences(handle, sequences);
}


void SkyModem::store_sequence_numbers()
{
	ofstream sequence_file("sequences");
	if (!sequence_file) {
		cerr << "Failed to open sequence file for writing" << endl;
		return;
	}

	int32_t sequences[2 * SKY_NUM_VIRTUAL_CHANNELS];
	sky_hmac_dump_sequences(handle, sequences);

	for (int i = 0; i < 2 * SKY_NUM_VIRTUAL_CHANNELS; i++)
		sequence_file << sequences[i] << " ";
}


void SkyModem::configureSDR(SoapySDR::Device *sdr)
{
	cerr << "Configuring USRP GPIO" << endl;
	unsigned int gpio_mask = 0x100;
	sdr->writeGPIO("FP0:CTRL", gpio_mask, gpio_mask);
	sdr->writeGPIO("FP0:DDR", gpio_mask, gpio_mask);
	sdr->writeGPIO("FP0:ATR_0X", 0, gpio_mask);
	sdr->writeGPIO("FP0:ATR_RX", 0, gpio_mask);
	sdr->writeGPIO("FP0:ATR_TX", gpio_mask, gpio_mask);
	sdr->writeGPIO("FP0:ATR_XX", gpio_mask, gpio_mask);
}


int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	SkyModem modem;
	return modem.run();
}