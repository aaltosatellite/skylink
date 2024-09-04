#ifndef __UNITS_H__
#define __UNITS_H__

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "narwhal.h"

#include "skylink/skylink.h"
#include "skylink/crc.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/fec.h"
#include "skylink/hmac.h"
#include "skylink/mac.h"
#include "skylink/utilities.h"
#include "skylink/reliable_vc.h"
#include "skylink/diag.h"
#include "skylink/sequence_ring.h"
#include "skylink/element_buffer.h"
#include "skylink/skylink.h"
#include "tools.h"

// Declare test suite groups
DECLARE_GROUP(frames);
DECLARE_GROUP(vc);
DECLARE_GROUP(mac);
DECLARE_GROUP(arq);
DECLARE_GROUP(hmac);
DECLARE_GROUP(fec);
DECLARE_GROUP(crc);

// Declare various parameters
DECLARE_PARAM(ticks, sky_tick_t);
DECLARE_PARAM(arq_sequence, sky_arq_sequence_t);
DECLARE_PARAM(arq_mask, sky_arq_mask_t);
DECLARE_PARAM(payload_length, unsigned int);
DECLARE_PARAM(frame_length, unsigned int);


#define ARRAY_SZ(array) (sizeof(array) / sizeof(array[0]))

#define MAX_ARQ_SEQUENCE ((1 << (8 * sizeof(sky_arq_sequence_t))) - 1)


extern const uint8_t key_a[32];
extern const uint8_t key_b[32];

extern const SkyHMACKey keys_a[1];
extern const SkyHMACKey keys_b[1] ;
extern const SkyHMACKey keys_ab[2];
extern const SkyHMACKey keys_ba[2];


extern const SkyConfig default_config;
/*
 * Write the default configs
 */
void default_configg(SkyConfig* config);

/*
 * Fill TX frame with
 */
void fill_random_payload(SkyTransmitFrame *tx_frame, unsigned int payload_len);

/*
 * Corrupt the given frame with exactly N byte errors.
 */
void corrupt_frame(SkyRadioFrame *frame, unsigned int byte_errors);

/*
 * Corrupt the given data with exactly N byte errors.
 */
void corrupt(uint8_t *data, unsigned int data_len, unsigned int byte_errors);

/*
 * Initialize
 */
void units_init_tx_frame(SkyRadioFrame *frame, SkyTransmitFrame *tx_frame);

/*
 * Force the length of the
 */
void units_set_tx_frame_length(SkyTransmitFrame* tx_frame, unsigned int frame_length);

/*
 *
 */
sky_tick_t units_advance_ticks(sky_tick_t ticks);

/*
 * Function to mimic bevahiour of sky_rx() function in the beginning.
 * The function does basic checks for the header just like in sky_rx() and
 * initializes SkyParsedFrame for next processing steps.
 */

int units_start_parsing(SkyRadioFrame *frame, SkyParsedFrame *parsed);

// Create payload of given length.
u_int8_t *create_payload(int length);

int get_cycle(SkyMAC *mac);


#endif /* __UNITS_H__ */