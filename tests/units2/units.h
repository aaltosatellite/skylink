#ifndef __UNITS_H__
#define __UNITS_H__


#include "narwhal.h"

#include "skylink/skylink.h"
#include "skylink/frame.h"
#include "skylink/fec.h"
#include "skylink/hmac.h"
#include "skylink/utilities.h"
#include "skylink/reliable_vc.h"
#include "skylink/diag.h"
#include "skylink/sequence_ring.h"
#include "skylink/element_buffer.h"
#include "skylink/skylink.h"
#include "tools.h"

#define ARRAY_SZ(array) (sizeof(array) / sizeof(array[0]))

#define MAX_ARQ_SEQUENCE ((1 << (8 * sizeof(sky_arq_sequence_t))) - 1)

/*
 * Write the default configs
 */
void default_config(SkyConfig* config);

/*
 * Corrupt the given data with exactly N byte errors.
 */
void corrupt(uint8_t *data, unsigned int data_len, unsigned int byte_errors);

/*
 * Initialize
 */
void init_tx(SkyRadioFrame *frame, SkyTransmitFrame *tx_frame);

/*
 * Function to mimic bevahiour of sky_rx() function in the beginning.
 * The function does basic checks for the header just like in sky_rx() and
 * initializes SkyParsedFrame for next processing steps.
 */
int start_parsing(SkyRadioFrame *frame, SkyParsedFrame *parsed);

#endif /* __UNITS_H__ */