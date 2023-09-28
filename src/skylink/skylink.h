#ifndef __SKYLINK_H__
#define __SKYLINK_H__

#include <stdint.h>

//============ DEFINES =================================================================================================
//======================================================================================================================


/*
 * Return codes
 */
#define SKY_RET_OK                          (0)

// RX
#define SKY_RET_INVALID_ENCODED_LENGTH      (-2)
#define SKY_RET_INVALID_PLAIN_LENGTH        (-3)
#define SKY_RET_INVALID_VERSION             (-4)
#define SKY_RET_INVALID_VC                  (-5)
#define SKY_RET_INVALID_EXT_LENGTH          (-6)
#define SKY_RET_REDUNDANT_EXTENSIONS        (-7)
#define SKY_RET_FILTERED_BY_IDENTITY        (-8)

// FEC
#define SKY_RET_GOLAY_FAILED                (-10)
#define SKY_RET_GOLAY_MISCONFIGURED         (-11)
#define SKY_RET_RS_FAILED                   (-12)
#define SKY_RET_RS_INVALID_LENGTH           (-13)

// MAC
#define SKY_RET_INVALID_MAC_WINDOW_SIZE     (-21)
#define SKY_RET_INVALID_MAC_REMAINING_SIZE  (-22)

// AUTH
#define SKY_RET_AUTH_FAILED                 (-30)
#define SKY_RET_AUTH_MISSING                (-31)
#define SKY_RET_EXCESSIVE_HMAC_JUMP         (-33)
#define SKY_RET_FRAME_TOO_LONG_FOR_HMAC     (-34)
#define SKY_RET_FRAME_TOO_SHORT_FOR_HMAC    (-35)

// PACKET
#define SKY_RET_NO_SPACE_FOR_PAYLOAD        (-40)
#define SKY_RET_UNKNOWN_EXTENSION           (-41)
#define SKY_RET_EXT_DECODE_FAIL             (-42)
#define SKY_RET_INVALID_EXT_TYPE            (-43)

// SEQUENCE RING
#define SKY_RET_RING_EMPTY                  (-50)
#define SKY_RET_RING_INVALID_SEQUENCE       (-51)
#define SKY_RET_RING_BUFFER_FULL            (-53)
#define SKY_RET_RING_RING_FULL              (-54)
#define SKY_RET_RING_PACKET_ALREADY_IN      (-55)
#define SKY_RET_RING_CANNOT_RECALL          (-56)
#define SKY_RET_RING_RESEND_FULL            (-57)
#define SKY_RET_RING_INVALID_ACKNOWLEDGE    (-58)
#define SKY_RET_RING_SEQUENCES_DETACHED     (-59)
#define SKY_RET_RING_SEQUENCES_OUT_OF_SYNC  (-60)
#define SKY_RET_TOO_LONG_PAYLOAD  			(-61)

// SYSTEM
#define SKY_RET_MALLOC_FAILED               (-70)

// ELEMENT BUFFER
#define SKY_RET_EBUFFER_INVALID_INDEX		(-110)
#define SKY_RET_EBUFFER_CHAIN_CORRUPTED		(-111)
#define SKY_RET_EBUFFER_NO_SPACE			(-112)
#define SKY_RET_EBUFFER_TOO_LONG_PAYLOAD	(-113)



/*
 * Number of virtual channels.
 * Can be modified for specific mission but the number must be from 1 to 4.
 */
#define SKY_NUM_VIRTUAL_CHANNELS            (4)


//============ TYPES ===================================================================================================
//======================================================================================================================

/* ARQ sequence number type  */
//typedef uint8_t sky_arq_sequence_t;
typedef uint16_t sky_arq_sequence_t;

/* Element buffer types */
typedef uint16_t sky_element_idx_t;
typedef uint16_t sky_element_length_t;



// Declare all structs so that we can start creating pointers
typedef struct sky_all *SkyHandle;
typedef struct sky_mac_s SkyMAC;
typedef struct sky_hmac SkyHMAC;
typedef struct sky_conf SkyConfig;
typedef struct sky_virtual_channel_s SkyVirtualChannel;
typedef struct sky_diag SkyDiagnostics;
typedef struct sky_radio_frame SkyRadioFrame;
typedef struct sky_element_buffer_s SkyElementBuffer;
typedef struct sky_send_ring_s SkySendRing;
typedef struct sky_rcv_ring_s SkyRcvRing;

/* Virtual Channel State */
typedef struct __attribute__((__packed__)) {
	/*
	 * ARQ state
	 */
	uint16_t state;

	/*
	 * Number of free slots in send ring.
	 */
	uint16_t free_tx_slots;

	/*
	 * Number of frames in the buffer waiting to be sent
	 * In reliable mode non-acknownledged frame are included.
	 */
	uint16_t tx_frames;

	/*
	 * Number of frames ready for reading.
	 */
	uint16_t rx_frames;

	/*
	 * Session identifier for the current ARQ connection
	 */
	uint32_t session_identifier;

} SkyVCState;


/* A state information struct provided for higher level software stack. */
typedef struct __attribute__((__packed__)) {
	SkyVCState vc[SKY_NUM_VIRTUAL_CHANNELS];
}  __attribute__ ((aligned (2))) SkyState;


/* Struct to store pointers to all the data structures related to a protocol instance. */
struct sky_all {
	SkyConfig*          conf;                 // Configuration
	SkyDiagnostics*	    diag;                 // Diagnostics
	SkyVirtualChannel*  virtual_channels[SKY_NUM_VIRTUAL_CHANNELS]; // ARQ capable buffers
	SkyMAC*             mac;                  // MAC state
	SkyHMAC*            hmac;                 // HMAC authentication state
};

/* Idenfity filter callback function type */
typedef int (*SkyReceiveFilterCallback)(const SkyHandle, const uint8_t *, unsigned int);

//============ FUNCTIONS ===============================================================================================
//======================================================================================================================

/*
 * Create new Skylink protocol instance based on the configuration struct.
 *
 * Args:
 *    config: Pointer to Skylink configuration struct.
 *            The config struct is not copied and it must outlive the instance created.
 */
SkyHandle sky_create(SkyConfig* config);

/*
 * Destroy Skylink instance and free all of its memory.
 *
 * Args:
 *    self: Pointer to Skylink instance to be destroyed.
 */
void sky_destroy(SkyHandle self);

/*
 * Get skylink protocol state
 *
 * Args:
 *    self: Pointer to Skylink instance
 *    state: Pointer to Skylink state object were current protocol state will be stored.
 */
void sky_get_state(SkyHandle self, SkyState* state);

/*
 * Generate a new frame to be sent. The frame won't have any FEC or Golay included yet.
 *
 * Args:
 *    self: Pointer to Skylink instance
 *    frame: Pointer to Skylink Radio Frame struct to which the new frame will be created.
 *
 * Returns:
 *    <0 if there was an error.
 *    0 if there's nothing to be sent.
 *    1 if there's something to be sent and the frame was written to given structure.
 */
int sky_tx(SkyHandle self, SkyRadioFrame *frame);

/*
 * Generate a new frame to be sent.
 * The frame will have the FEC included.
 *
 * Args:
 *    self: Pointer to Skylink instance
 *    frame: Pointer to Skylink Radio Frame struct to which the new frame will be created.
 *
 * Returns:
 *    <0 if there was an error.
 *    0 if there's nothing to be sent.
 *    1 if there's something to be sent and the frame was written to given structure.
 */
int sky_tx_with_fec(SkyHandle self, SkyRadioFrame *frame);

/*
 * Generate a new frame to be sent.
 * The frame will have the FEC and Golay header included.
 *
 * Args:
 *    self: Pointer to Skylink instance
 *    frame: Pointer to Skylink Radio Frame struct to which the new frame will be created.
 *
 * Returns:
 *    <0 if there was an error.
 *    0 if there's nothing to be sent.
 *    1 if there's something to be sent and the frame was written to given structure.
 */
int sky_tx_with_golay(SkyHandle self, SkyRadioFrame *frame);

/*
 * Pass received frame for the protocol logic.
 * The frame doesn't have FEC or Golay included.
 *
 * Args:
 *    self: Skylink handle
 *    frame: Received radio frame
 * Returns:
 *    <0 if there was an error.
 *    0 if there was no error while processing the frame.
 */
int sky_rx(SkyHandle self, const SkyRadioFrame *frame);

/*
 * Pass received frame for the protocol logic.
 * The frame has FEC included but no Golay header.
 *
 * Args:
 *    self: Skylink handle
 *    frame: Received radio frame
 * Returns:
 *    <0 if there was an error.
 *    0 if there was no error while processing the frame.
 */
int sky_rx_with_fec(SkyHandle self, SkyRadioFrame *frame);

/*
 * Pass received frame for the protocol logic.
 * The frame will have the FEC and Golay header included.
 *
 * Args:
 *    self: Skylink handle
 *    frame: Received radio frame
 * Returns:
 *    <0 if there was an error.
 *    0 if there was no error while processing the frame.
 */
int sky_rx_with_golay(SkyHandle self, SkyRadioFrame *frame);

/*
 * Start the ARQ process connecting procedure
 *
 * Args:
 *    vchannel: Pointer to virtual channel
 */
int sky_vc_arq_connect(SkyVirtualChannel *vchannel);

/*
 * Disconnect/flush ARQ process
 *
 * Args:
 *    vchannel: Pointer to virtual channel
 */
int sky_vc_arq_disconnect(SkyVirtualChannel *vchannel);

/* */
void sky_set_receive_filter(SkyHandle self, SkyReceiveFilterCallback *callback);

#endif // __SKYLINK_H__
