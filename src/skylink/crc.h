#ifndef __SKYLINK_CRC_H__
#define __SKYLINK_CRC_H__

#include "skylink/skylink.h"

/*
 * Calculate CRC-32 checksum of the given data.
 *
 * Args:
 *     buf: Pointer to the data
 *     len: Length of the data
 *
 * Returns:
 *     Checksum as an uint32
 */
uint32_t sky_crc32(const uint8_t *buf, unsigned int len);

/*
 * Extend the frame with CRC-32 checksum.
 *
 * Args:
 *     frame: The frame to be extended with CRC
 */
int sky_extend_with_crc32(SkyRadioFrame *frame);

/*
 * Check whether the CRC in the end of the frame matches with the data.
 * Length of the checksum is decremented from the frame length.
 *
 * Args:
 *     frame: Frame to be checked for data integrity.
 *
 * Returns:
 *     0 on success, negative on failure.
 */
int sky_check_crc32(SkyRadioFrame *frame);


#endif /* __SKYLINK_CRC_H__ */
