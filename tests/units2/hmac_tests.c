// Unit tests for the HMAC Blake3 implementation.

#include "units.h"

// Functions to create necessary parts for tests. Removes having to copy-paste code.
SkyConfig* config_create(){
    SkyConfig* config = SKY_MALLOC(sizeof(SkyConfig));
    default_config(config);
    return config;
}

SkyHandle handle_create(SkyConfig* config){
    SkyHandle handle = sky_create(config);
    return handle;
}

SkyHMAC *hmac_create(SkyHandle handle){
    SkyHMAC* hmac = sky_hmac_create(&handle->conf->hmac);
    return hmac;
}

// Test creating an HMAC context.
TEST(create_HMAC){
    // Malloc SkyConfig struct and set default config.
    SkyConfig* config = config_create();
    // Create SkyHandle struct.
    SkyHandle handle = handle_create(config);
    // Create SkyHMAC struct.
    SkyHMAC* hmac = hmac_create(handle);
    // Check that the struct is not NULL.
    ASSERT(hmac != NULL, "Create HMAC failed.");
    // Assert values of the struct.
    ASSERT(hmac->key_len == 32, "HMAC key len should be 32, was: %d", hmac->key_len);
    ASSERT(memcmp(hmac->key, handle->conf->hmac.key, 32) == 0, "HMAC key should be equal to config key.");
    // Free the SkyConfig struct.
    SKY_FREE(config);
    // Destroy HMAC struct.
    sky_hmac_destroy(hmac);
    // Destroy SkyHandle struct.
    sky_destroy(handle);

}

// TODO: Add sequences to tests.
TEST(HMAC_extend){
    // Malloc SkyConfig struct and set default config.
    SkyConfig* config = config_create();
    // Create SkyHandle struct.
    SkyHandle handle = handle_create(config);
    // Set require authentication for VC0 to 1.
    handle->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;

    // Create initialize frames.
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    SkyParsedFrame parsed;
    memset(&parsed, 0, sizeof(SkyParsedFrame));
    init_tx(&frame, &TXframe);
    parsed.payload_len = 50;
    parsed.hdr.vc = 0;
    parsed.hdr.flag_authenticated = 1;
    ASSERT((handle->conf->vc[0].require_authentication & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) != 0);
    ASSERT((parsed.hdr.flags & SKY_FLAG_AUTHENTICATED) != 0, "Returned %d", parsed.hdr.flags);
    // Calculate hash for randomly filled data up to 200 bytes. Pointer is at byte after final written index.
    TXframe.frame->length = 200;
    TXframe.ptr = &TXframe.frame->raw[200];
    sky_hmac_extend_with_authentication(handle, &TXframe);
    // Debug hash print.
    //const uint8_t *frame_hash = &frame.raw[frame.length - SKY_HMAC_LENGTH];
    //ASSERT(0, "HASH: %x%x%x%x", frame_hash[0], frame_hash[1], frame_hash[2], frame_hash[3]);
    ASSERT(frame.length == 204, "Returned %d", frame.length); // Frame length OK?
    ASSERT(parsed.hdr.vc == 0, "Returned %d", parsed.hdr.vc); // VC OK?
    ASSERT((handle->conf->vc[0].require_authentication & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) != 0);
    ASSERT((parsed.hdr.flags & SKY_FLAG_AUTHENTICATED) != 0, "Returned %d", parsed.hdr.flags);
    // Check authentication.    
    int auth = sky_hmac_check_authentication(handle, TXframe.frame, &parsed);
    ASSERT(auth == 0, "Returned %d", auth); // Auth OK?
    ASSERT(parsed.payload_len == 46, "Returned %d", parsed.payload_len); // Payload length OK?, if HMAC passed, payload length should be original-4.
    // Free the SkyConfig struct.
    SKY_FREE(config);
    // Destroy SkyHandle struct.
    sky_destroy(handle);
}

TEST(HMAC_invalid_key){
    // Malloc SkyConfig struct and set default config.
    SkyConfig* config1 = config_create();
    SkyConfig* config2 = config_create();
    config2->hmac.key[0] = 0x20;
    // Create SkyHandle struct.
    SkyHandle handle1 = handle_create(config1);
    SkyHandle handle2 = handle_create(config2);
    // Set require authentication for VC0 to 1.
    handle1->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    handle2->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    // Create initialize frames.
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    SkyParsedFrame parsed;
    memset(&parsed, 0, sizeof(SkyParsedFrame));
    init_tx(&frame, &TXframe);
    parsed.hdr.flag_authenticated = 1;
    parsed.hdr.vc = 0;
    // Calculate hash for randomly filled data up to 200 bytes. Pointer is at byte after final written index.
    TXframe.frame->length = 200;
    TXframe.ptr = &TXframe.frame->raw[200];
    sky_hmac_extend_with_authentication(handle1, &TXframe);
    parsed.payload_len = 50;
    // Debug hash print.
    //const uint8_t *frame_hash = &frame.raw[frame.length - SKY_HMAC_LENGTH];
    //ASSERT(0, "HASH: %x%x%x%x", frame_hash[0], frame_hash[1], frame_hash[2], frame_hash[3]);
    ASSERT(frame.length == 204, "Returned %d", frame.length); // Frame length OK?
    ASSERT(parsed.hdr.vc == 0, "Returned %d", parsed.hdr.vc); // VC OK?
    ASSERT((handle1->conf->vc[0].require_authentication & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) != 0);
    ASSERT((handle2->conf->vc[0].require_authentication & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) != 0);
    ASSERT((parsed.hdr.flags & SKY_FLAG_AUTHENTICATED) != 0, "Returned %d", parsed.hdr.flags);
    // Check authentication.    
    int auth = sky_hmac_check_authentication(handle2, TXframe.frame, &parsed);
    ASSERT(auth == SKY_RET_AUTH_FAILED, "Returned %d", auth); // Auth OK?
    ASSERT(parsed.payload_len == 50, "Returned %d", parsed.payload_len); // Payload length OK?, if HMAC passed, payload length should be original-4.
    // Free the SkyConfig struct.
    SKY_FREE(config1);
    SKY_FREE(config2);
    // Destroy SkyHandle struct.
    sky_destroy(handle1);
    sky_destroy(handle2);
}

// Test that the HMAC is not calculated if authentication is not required. Should allow differing hmac keys to go through checking authentication.
TEST(no_authentication){
    // Create configs with different keys.
    SkyConfig* config1 = config_create();
    SkyConfig* config2 = config_create();
    config2->hmac.key[0] = 0x20;
    // Create SkyHandle structs.
    SkyHandle handle1 = handle_create(config1);
    SkyHandle handle2 = handle_create(config2);
    // Set require authentication for VC0 to 0.
    handle1->conf->vc[0].require_authentication = 0;
    handle2->conf->vc[0].require_authentication = 0;
    // Create initialize frames.
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    SkyParsedFrame parsed;
    memset(&parsed, 0, sizeof(SkyParsedFrame));
    init_tx(&frame, &TXframe);
    parsed.hdr.flags |= SKY_FLAG_AUTHENTICATED;
    parsed.hdr.vc = 0;
    parsed.payload_len = 50;
    // Calculate hash for randomly filled data up to 200 bytes. Pointer is at byte after final written index.
    TXframe.frame->length = 200;
    TXframe.ptr = &TXframe.frame->raw[200];
    sky_hmac_extend_with_authentication(handle1, &TXframe);
    // Debug hash print.
    //const uint8_t *frame_hash = &frame.raw[frame.length - SKY_HMAC_LENGTH];
    //ASSERT(0, "HASH: %x%x%x%x", frame_hash[0], frame_hash[1], frame_hash[2], frame_hash[3]);
    ASSERT(frame.length == 204, "Returned %d", frame.length); // Frame length OK?
    ASSERT((handle1->conf->vc[0].require_authentication & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) == 0);
    ASSERT((handle2->conf->vc[0].require_authentication & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) == 0);
    // Check authentication.
    int auth = sky_hmac_check_authentication(handle2, TXframe.frame, &parsed);
    ASSERT(auth == 0, "Returned %d", auth); // Auth should be ok even though keys are different. // Sequence check still needs to be done.
    ASSERT(parsed.payload_len == 46, "Returned %d", parsed.payload_len); // Payload length OK?, if HMAC passed, payload length should be original-4.
    // Free the SkyConfig struct.
    SKY_FREE(config1);
    SKY_FREE(config2);
    // Destroy SkyHandle struct.
    sky_destroy(handle1);
    sky_destroy(handle2);
}

// Test flag placement in bitfield for SkyStaticHeader of parsed frame.
TEST(header_bits){
    SkyParsedFrame parsed;
    memset(&parsed, 0, sizeof(SkyParsedFrame));
    ASSERT(parsed.hdr.flags == 0, "Returned %d", parsed.hdr.flags);
    parsed.hdr.flag_authenticated = 1;
    ASSERT(parsed.hdr.flags == 0b00001000, "Returned %d", parsed.hdr.flags);
    parsed.hdr.flag_authenticated = 0;
    ASSERT(parsed.hdr.flags == 0, "Returned %d", parsed.hdr.flags);
    parsed.hdr.flag_arq_on = 1;
    ASSERT(parsed.hdr.flags == 0b00000100, "Returned %d", parsed.hdr.flags);
    parsed.hdr.flag_arq_on = 0;
    ASSERT(parsed.hdr.flags == 0, "Returned %d", parsed.hdr.flags);
    parsed.hdr.flag_has_payload = 1;
    ASSERT(parsed.hdr.flags == 0b00010000, "Returned %d", parsed.hdr.flags);
    parsed.hdr.flag_has_payload = 0;
    ASSERT(parsed.hdr.flags == 0, "Returned %d", parsed.hdr.flags);
    parsed.hdr.vc = 3;
    ASSERT(parsed.hdr.flags & 0b11, "Returned %d", parsed.hdr.flags);



/*
FIX NOTES:
Order of bit field "Flags" in SkyStaticHeader:
VC0 =                       0b00000000
VC1 =                       0b00000001
VC2 =                       0b00000010
VC3 =                       0b00000011
SKY_FLAG_ARQ_ON =           0b00000100
SKY_FLAG_AUTHENTICATED =    0b00001000
SKY_FLAG_HAS_PAYLOAD =      0b00010000
Sequence control bits =     0b01100000
Reserved =                  0b10000000
Fixed so that SKY_FLAG_ARQ_ON is now 0b00000100, SKY_FLAG_AUTHENTICATED is 0b00001000 and SKY_FLAG_HAS_PAYLOAD is 0b00010000.
Previously bits were offset.
*/


}