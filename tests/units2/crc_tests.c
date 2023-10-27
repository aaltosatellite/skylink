// Skylink CRC implementation tests

#include "units.h"

// Extend a frame with a CRC and check it.
TEST(crc_extension){
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);

    // Create initialize frames.
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    init_tx(&frame, &TXframe);
    fillrand(TXframe.frame, TXframe.frame->length);
    TXframe.frame->length = 200;
    sky_extend_with_crc32(TXframe.frame);
    // Check that the frame length is correct.
    ASSERT(TXframe.frame->length == 200 + sizeof(uint32_t), "CRC extension failed. Length should be %d, was %d", 200 + sizeof(uint32_t), TXframe.frame->length);
    // Check that the CRC is correct.
    ASSERT(sky_check_crc32(TXframe.frame) == SKY_RET_OK, "CRC check failed.");
    // Check that the frame length is correct.
    ASSERT(TXframe.frame->length == 200, "CRC extension failed.");
    // Check that the CRC is correct.
    ASSERT(sky_check_crc32(TXframe.frame) == SKY_RET_CRC_INVALID_CHECKSUM, "CRC calculation somehow passed even though it should be removed.");
    // Free the SkyConfig struct.
    SKY_FREE(config);
    // Destroy SkyHandle struct.
    sky_destroy(handle);
    
}

// Invalid CRC check.
TEST(invalid_crc){
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Set require authentication for VC0 to 1.
    handle->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;

    // Create initialize frames.
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    init_tx(&frame, &TXframe);
    fillrand(TXframe.frame, TXframe.frame->length);
    TXframe.frame->length = 200;
    sky_extend_with_crc32(TXframe.frame);
    // To make sure random fill doesn't make the CRC valid.
    if(TXframe.frame->raw[TXframe.frame->length - 1] == 0x00){
        TXframe.frame->raw[TXframe.frame->length - 1] = 0xFF;
    }else{
        TXframe.frame->raw[TXframe.frame->length - 1] = 0x00;
    }
    // Check that the frame length is correct.
    ASSERT(TXframe.frame->length == 200 + sizeof(uint32_t), "CRC extension failed. Length should be %d, was %d", 200 + sizeof(uint32_t), TXframe.frame->length);
    // Check that the CRC is invalid.
    ASSERT(sky_check_crc32(TXframe.frame) == SKY_RET_CRC_INVALID_CHECKSUM, "CRC check succeeded even though it should be invalid.");
    // Frame length should be the same.
    ASSERT(TXframe.frame->length == 200 + sizeof(uint32_t), "CRC extension failed. Length should be %d, was %d", 200 + sizeof(uint32_t), TXframe.frame->length);
    // Test changing first byte.
    TXframe.frame->length = 200;
    // Extend with CRC.
    sky_extend_with_crc32(TXframe.frame);
    // Change data.
    if(TXframe.frame->raw[0] == 0x00){
        TXframe.frame->raw[0] = 0xFF;
    }else{
        TXframe.frame->raw[0] = 0x00;
    }
    // Check that the CRC is invalid.
    ASSERT(sky_check_crc32(TXframe.frame) == SKY_RET_CRC_INVALID_CHECKSUM, "CRC check succeeded even though it should be invalid.");
    // Frame length should be the same.
    ASSERT(TXframe.frame->length == 200 + sizeof(uint32_t), "CRC extension failed. Length should be %d, was %d", 200 + sizeof(uint32_t), TXframe.frame->length);

    // Free the SkyConfig struct.
    SKY_FREE(config);
    // Destroy SkyHandle struct.
    sky_destroy(handle);
}