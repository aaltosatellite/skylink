#include "units.h"


TEST(fec_successful)
{
	SkyRadioFrame* frame = sky_frame_create();
	SkyDiagnostics* diag = sky_diag_create();

	int length = randint_i32(16+8, RS_MSGLEN);
	int n_corrupt_bytes = randint_i32(0, 16); // RS(223,256) can correct up to 16 byte errors.

	// Make a reference payload.
	uint8_t* ref = malloc(RS_MSGLEN*3);
	fillrand(ref, RS_MSGLEN*3);
	memcpy(frame->raw, ref, length);
	frame->length = length;


	// fec encode it.
	sky_fec_encode(frame);


	// Take a reference of the encoded product.
	uint8_t* encoded_ref = malloc(frame->length);
	memcpy(encoded_ref, frame->raw, frame->length);

	// Corrupt the data
	corrupt(frame->raw, frame->length, n_corrupt_bytes);

	// Assert that we in fact did what we meant.
	if (n_corrupt_bytes != 0)
		ASSERT(memcmp(encoded_ref, frame->raw, frame->length) != 0);
	else
		ASSERT(memcmp(encoded_ref, frame->raw, frame->length) == 0);


	//attempt fec decode.
	int ret = sky_fec_decode(frame, diag);
	if (ret == 0){
		//printf("SUCCESS:  %d bytes.\n",  n_corrupt_bytes);
	}
	if (ret != 0){
		printf("Failure to correct: %d bytes. (not necassarily a fail in test)\n", n_corrupt_bytes);
	}


	//Assert that the fec response was understood correctly...
	if(n_corrupt_bytes == 0){
		ASSERT(ret == 0);
		ASSERT(memcmp(ref, frame->raw, length) == 0);
	}
	if(ret == 0){
		ASSERT(memcmp(ref, frame->raw, length) == 0);
		ASSERT((int)frame->length == length);
	}
	if(ret != 0){
		ASSERT(memcmp(ref, frame->raw, length) != 0);
	}


	sky_frame_destroy(frame);
	sky_diag_destroy(diag);
	free(encoded_ref);
	free(ref);
}

/*
 * Test FEC decoding with too short and too long frames.
 */
TEST(fec_wrong_length)
{
	int ret;
	SkyRadioFrame frame;
	frame.rx_time_ticks = 0;
	frame.length = 0;
	fillrand(frame.raw, sizeof(frame.raw));
	SkyDiagnostics diag = { 0 };

	// Too short frame to be decoded
	frame.length = 31;
	ret = sky_fec_decode(&frame, &diag);
	ASSERT(ret == SKY_RET_RS_INVALID_LENGTH, "ret: %d", ret);
	//NOTE: FEC Error counter is not incremented for too short or long frames
	ASSERT(diag.rx_fec_ok == 0);
	ASSERT(diag.rx_fec_fail == 1);

	// Too long of a frame
	frame.rx_time_ticks = 0;
	frame.length = 256;
	ret = sky_fec_decode(&frame, &diag);
	ASSERT(ret == SKY_RET_RS_INVALID_LENGTH, "ret: %d", ret);
	ASSERT(diag.rx_fec_ok == 0);
	ASSERT(diag.rx_fec_fail == 2);

	// Too long frame to be encoded
	frame.length = 255;
	ret = sky_fec_encode(&frame);
	ASSERT(ret == SKY_RET_RS_INVALID_LENGTH, "ret: %d", ret);

	// Successful encode with maximum length message
	frame.length = RS_MSGLEN;
	ret = sky_fec_encode(&frame);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(frame.length == 255);

	// Successful decode with maximum length coded message
	ret = sky_fec_decode(&frame, &diag);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(frame.length == RS_MSGLEN);
	ASSERT(diag.rx_fec_ok == 1);
	ASSERT(diag.rx_fec_fail == 2);
	ASSERT(diag.rx_fec_octs == RS_MSGLEN + RS_PARITYS);

	// Encode a shorter frame
	frame.length = 100;
	ret = sky_fec_encode(&frame);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(frame.length == 100 + RS_PARITYS);

	// Try to decode truncated frame
	// Frame length must be off by more than +/-16 to cause RS decode failure.
	{
		SkyRadioFrame copy_frame = frame;
		copy_frame.length = 100 + RS_PARITYS - 17;
		ret = sky_fec_decode(&copy_frame, &diag);
		ASSERT(ret == SKY_RET_RS_FAILED, "ret: %d", ret);
		ASSERT(diag.rx_fec_ok == 1);
		ASSERT(diag.rx_fec_fail == 3);
	}

	// Try to decode frame with extra trailing byte
	{
		SkyRadioFrame copy_frame = frame;
		copy_frame.length = 100 + RS_PARITYS + 17;
		ret = sky_fec_decode(&copy_frame, &diag);
		ASSERT(ret == SKY_RET_RS_FAILED, "ret: %d", ret);
		ASSERT(diag.rx_fec_errs == 0, "Incorrect number of errors corrected: %d", diag.rx_fec_errs);
		ASSERT(diag.rx_fec_ok == 1);
		ASSERT(diag.rx_fec_fail == 4);
	}
}
