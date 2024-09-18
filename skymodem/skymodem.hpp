#pragma once

#include <suo.hpp>
#include <signal-io/soapysdr_io.hpp>
#include <modem/demod_fsk_mfilt.hpp>
#include <modem/demod_gmsk_cont.hpp>
#include <modem/mod_gmsk.hpp>
#include <frame-io/zmq_interface.hpp>
#include <framing/golay_framer.hpp>
#include <framing/golay_deframer.hpp>
#ifdef USE_PORTHOUSE_TRACKER
#include <misc/porthouse_tracker.hpp>
#endif

#include "vc_interface.hpp"
#include <SoapySDR/Device.hpp>

#include "skylink.hpp"


class SkyModem {
public:

	SkyModem();
	~SkyModem();

	int run();

	void tick(suo::Timestamp now);
	void receiver_locked(bool locked, suo::Timestamp now);
	void frame_transmit(suo::Frame& frame, suo::Timestamp now);
	void frame_received(suo::Frame& frame, suo::Timestamp now);

private:
	void load_sequence_numbers();
	void store_sequence_numbers();
	void configureSDR(SoapySDR::Device *sdr);

	SkyConfig config;
	SkyHandle handle;
	sky_tick_t last_sequence_refresh;

	suo::SoapySDRIO* sdr;

	suo::GMSKContinousDemodulator *receiver_9k6;
	suo::GolayDeframer *deframer_9k6;

#ifdef MULTIMODE
	suo::GMSKContinousDemodulator *receiver_19k2;
	suo::GolayDeframer *deframer_19k2;

	suo::GMSKContinousDemodulator *receiver_38k4;
	suo::GolayDeframer *deframer_38k4;
#endif

	suo::GMSKModulator *modulator;
	suo::GolayFramer *framer;

	suo::ZMQPublisher *zmq_output;
	VCInterface* vc_interface;

#ifdef USE_PORTHOUSE_TRACKER
	suo::PorthouseTracker *tracker;
#endif
};

