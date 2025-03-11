import numpy as np
from numba import njit, prange


#DEFAULT_SYNCHWORD = 0x930B51DE
DEFAULT_SYNCHWORD = 0x1ACFFC1D
DEFAULT_SYNCHWORD_BITS = np.array( [int(x) for x in ("0"*32+bin(DEFAULT_SYNCHWORD)[2:])[-32:]], dtype=np.int32)
DEFAULT_SYNCHWORD_LEN = 32
# = CCSDS TM RANDOMIZER ======================================================================================================================================================================
# = CCSDS TM RANDOMIZER ======================================================================================================================================================================
ccsds_tm_whitening_bytes = np.array([
0xff, 0x48, 0xe,  0xc0, 0x9a, 0xd,  0x70, 0xbc, 0x8e, 0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce, 0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e,
0xa,  0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1, 0xfe, 0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79, 0x1c, 0x59, 0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c,
0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e, 0x7c, 0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63, 0xfd, 0x20, 0x3b, 0x2,  0x68, 0x35, 0xc2, 0xf2,
0x38, 0xb2, 0x4e, 0xb6, 0x9e, 0xdd, 0x1b, 0x39, 0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8, 0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7,
0xfa, 0x40, 0x76, 0x4,  0xd0, 0x6b, 0x85, 0xe4, 0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72, 0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0,
0x50, 0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f, 0xf4, 0x80, 0xec, 0x9,  0xa0, 0xd7, 0xb,  0xc8, 0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5,
0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0, 0xa1, 0xf,  0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f, 0xe9, 0x1,  0xd8, 0x13, 0x41, 0xae, 0x17, 0x91,
0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb, 0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1, 0x42, 0x1e, 0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f,
0xd2, 0x3,  0xb0, 0x26, 0x83, 0x5c, 0x2f, 0x23, 0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96, 0xa5, 0xdf, 0x73, 0xc,  0xa8, 0xaf, 0xcf, 0x82,
0x84, 0x3c, 0x62, 0x25, 0x33, 0x7a, 0xac, 0x7f, 0xa4, 0x7,  0x60, 0x4d, 0x6,  0xb8, 0x5e, 0x47, 0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d,
0x4b, 0xbe, 0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x5,  0x8,  0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff, 0x48, 0xe,  0xc0, 0x9a, 0xd,  0x70, 0xbc, 0x8e,
0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce, 0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e, 0xa,  0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1, 0xfe,
0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79, 0x1c, 0x59, 0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c, 0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e, 0x7c, 0x14,
0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63, 0xfd, 0x20, 0x3b, 0x2,  0x68, 0x35, 0xc2, 0xf2, 0x38, 0xb2, 0x4e, 0xb6, 0x9e, 0xdd, 0x1b, 0x39, 0x6a,
0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8, 0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7, 0xfa, 0x40, 0x76, 0x4,  0xd0, 0x6b, 0x85, 0xe4, 0x71,
0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72, 0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0, 0x50, 0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f, 0xf4,
0x80, 0xec, 0x9,  0xa0, 0xd7, 0xb,  0xc8, 0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5, 0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0, 0xa1,
0xf,  0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f, 0xe9, 0x1,  0xd8, 0x13, 0x41, 0xae, 0x17, 0x91, 0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb, 0x52,
0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1, 0x42, 0x1e, 0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f, 0xd2, 0x3,  0xb0, 0x26, 0x83, 0x5c, 0x2f, 0x23, 0x8b,
0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96, 0xa5, 0xdf, 0x73, 0xc,  0xa8, 0xaf, 0xcf, 0x82, 0x84, 0x3c, 0x62, 0x25, 0x33, 0x7a, 0xac, 0x7f, 0xa4,
0x7,  0x60, 0x4d, 0x6,  0xb8, 0x5e, 0x47, 0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d, 0x4b, 0xbe, 0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x5,  0x8,
0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff, 0x48, 0xe,  0xc0, 0x9a, 0xd,  0x70, 0xbc, 0x8e, 0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce, 0x5a, 0x97,
0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e, 0xa,  0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1, 0xfe, 0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79, 0x1c, 0x59,
0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c, 0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e, 0x7c, 0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63, 0xfd, 0x20,
0x3b, 0x2,  0x68, 0x35, 0xc2, 0xf2, 0x38, 0xb2, 0x4e, 0xb6, 0x9e, 0xdd, 0x1b, 0x39, 0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8, 0x28, 0x43,
0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7, 0xfa, 0x40, 0x76, 0x4,  0xd0, 0x6b, 0x85, 0xe4, 0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72, 0xd4, 0xbb,
0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0, 0x50, 0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f, 0xf4, 0x80, 0xec, 0x9,  0xa0, 0xd7, 0xb,  0xc8, 0xe2, 0xc9,
0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5, 0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0, 0xa1, 0xf,  0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f, 0xe9, 0x1,
0xd8, 0x13, 0x41, 0xae, 0x17, 0x91, 0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb, 0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1, 0x42, 0x1e,
0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f, 0xd2, 0x3,  0xb0, 0x26, 0x83, 0x5c, 0x2f, 0x23, 0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96, 0xa5, 0xdf,
0x73, 0xc,  0xa8, 0xaf, 0xcf, 0x82, 0x84, 0x3c, 0x62, 0x25, 0x33, 0x7a, 0xac, 0x7f, 0xa4, 0x7,  0x60, 0x4d, 0x6,  0xb8, 0x5e, 0x47, 0x16, 0x49,
0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d, 0x4b, 0xbe, 0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x5,  0x8,  0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff, 0x48, 0xe,
0xc0, 0x9a, 0xd,  0x70, 0xbc, 0x8e, 0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce, 0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e, 0xa,  0x10, 0xf1,
0x88, 0x94, 0xcd, 0xea, 0xb1, 0xfe, 0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79, 0x1c, 0x59, 0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c, 0xb5, 0x2e, 0xfb,
0x98, 0x65, 0x45, 0x7e, 0x7c, 0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63, 0xfd, 0x20, 0x3b, 0x2,  0x68, 0x35, 0xc2, 0xf2, 0x38, 0xb2, 0x4e,
0xb6, 0x9e, 0xdd, 0x1b, 0x39, 0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8, 0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7, 0xfa, 0x40, 0x76,
0x4,  0xd0, 0x6b, 0x85, 0xe4, 0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72, 0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0, 0x50, 0x87, 0x8c,
0x44, 0xa6, 0x6f, 0x55, 0x8f, 0xf4, 0x80, 0xec, 0x9,  0xa0, 0xd7, 0xb,  0xc8, 0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5, 0xa9, 0x77, 0xdc,
0xc3, 0x2a, 0x2b, 0xf3, 0xe0, 0xa1, 0xf,  0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f, 0xe9, 0x1,  0xd8, 0x13, 0x41, 0xae, 0x17, 0x91, 0xc5, 0x92, 0x75,
0xb4, 0xf6, 0xe8, 0xd9, 0xcb, 0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1, 0x42, 0x1e, 0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f, 0xd2, 0x3,  0xb0,
0x26, 0x83, 0x5c, 0x2f, 0x23, 0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96, 0xa5, 0xdf, 0x73, 0xc,  0xa8, 0xaf, 0xcf, 0x82, 0x84, 0x3c, 0x62,
0x25, 0x33, 0x7a, 0xac, 0x7f, 0xa4, 0x7,  0x60, 0x4d, 0x6,  0xb8, 0x5e, 0x47, 0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d, 0x4b, 0xbe, 0xe6,
0x19, 0x51, 0x5f, 0x9f, 0x5,  0x8,  0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff, 0x48, 0xe,  0xc0, ],dtype=np.int32)

def CCSDS_TM_whitener_sequence(n):
	state = 0xff
	outp = 0
	bitarr = []
	bytearr = []
	for ii in range(n):
		outp = ((outp<<1) + (state & 1)) & 0xff
		if (ii%8) == 7:
			bytearr.append(outp)
		bitarr.append(state & 1)
		state = (state>>1) + ((((state >> 7) & 1) ^ ((state >> 5) & 1) ^ ((state >> 3) & 1) ^ (state & 1)) << 7)
	return np.array(bytearr, dtype=np.int32), np.array(bitarr, dtype=np.int8)
# = CCSDS TM RANDOMIZER ======================================================================================================================================================================
# = CCSDS TM RANDOMIZER ======================================================================================================================================================================



## PN9 SEQUENCE ==============================================================================================================================================================================
## PN9 SEQUENCE ==============================================================================================================================================================================
PN9_bytes = np.array([255, 225, 29, 154, 237, 133, 51, 36, 234, 122, 210, 57, 112, 151, 87, 10, 84, 125, 45, 216, 109, 13, 186, 143, 103, 89, 199, 162, 191, 52,
			 202, 24, 48, 83, 147, 223, 146, 236, 167, 21, 138, 220, 244, 134, 85, 78, 24, 33, 64, 196, 196, 213, 198, 145, 138, 205, 231, 209, 78, 9, 50,
			 23, 223, 131, 255, 240, 14, 205, 246, 194, 25, 18, 117, 61, 233, 28, 184, 203, 43, 5, 170, 190, 22, 236, 182, 6, 221, 199, 179, 172, 99, 209,
			 95, 26, 101, 12, 152, 169, 201, 111, 73, 246, 211, 10, 69, 110, 122, 195, 42, 39, 140, 16, 32, 98, 226, 106, 227, 72, 197, 230, 243, 104, 167,
			 4, 153, 139, 239, 193, 127, 120, 135, 102, 123, 225, 12, 137, 186, 158, 116, 14, 220, 229, 149, 2, 85, 95, 11, 118, 91, 131, 238, 227, 89, 214,
			 177, 232, 47, 141, 50, 6, 204, 212, 228, 183, 36, 251, 105, 133, 34, 55, 189, 97, 149, 19, 70, 8, 16, 49, 113, 181, 113, 164, 98, 243, 121, 180,
			 83, 130, 204, 197, 247, 224, 63, 188, 67, 179, 189, 112, 134, 68, 93, 79, 58, 7, 238, 242, 74, 129, 170, 175, 5, 187, 173, 65, 247, 241, 44, 235,
			 88, 244, 151, 70, 25, 3, 102, 106, 242, 91, 146, 253, 180, 66, 145, 155, 222, 176, 202, 9, 35, 4, 136, 152, 184, 218, 56, 82, 177, 249, 60, 218,
			 41, 65, 230, 226, 123, 240, 31, 222, 161, 217, 94, 56, 67, 162, 174, 39, 157, 3, 119, 121, 165, 64, 213, 215, 130, 221, 214, 160, 251, 120, 150,
			 117, 44, 250, 75, 163, 140, 1, 51, 53, 249, 45, 201, 126, 90, 161, 200, 77, 111, 88, 229, 132, 17, 2, 68, 76, 92, 109, 28, 169, 216, 124, 30, 237,
			 148, 32, 115, 241, 61, 248, 15, 239, 208, 108, 47, 156, 33, 81, 215, 147, 206, 129, 187, 188, 82, 160, 234, 107, 193, 110, 107, 208, 125, 60, 203,
			 58, 22, 253, 165, 81, 198, 128, 153, 154, 252, 150, 100, 63, 173, 80, 228, 166, 55, 172, 114, 194, 8, 1, 34, 38, 174, 54, 142, 84, 108, 62, 143,
			 118, 74, 144, 185, 248, 30, 252, 135, 119, 104, 182, 23, 206, 144, 168, 235, 73, 231, 192, 93, 94, 41, 80, 245, 181, 96, 183, 53, 232, 62, 158, 101,
			 29, 139, 254, 210, 40, 99, 192, 76, 77, 126, 75, 178, 159, 86, 40, 114, 211, 27, 86, 57, 97, 132, 0, 17, 19, 87, 27, 71, 42, 54, 159, 71, 59, 37,
			 200, 92, 124, 15, 254, 195, 59, 52, 219, 11, 103, 72, 212, 245, 164, 115, 224, 46, 175, 20, 168, 250, 90, 176, 219, 26, 116, 31, 207, 178, 142, 69,
			 127, 105, 148, 49, 96, 166, 38, 191, 37, 217, 79, 43, 20, 185, 233, 13, 171, 156, 48, 66, 128, 136, 137, 171, 141, 35, 21, 155, 207, 163, 157, 18,
			 100, 46, 190, 7, 255, 225, 29, 154, 237, 133, 51, 36, 234, 122, 210, 57, 112, 151, 87, 10, 84, 125, 45, 216, 109, 13, 186, 143, 103, 89, 199, 162,
			 191, 52, 202, 24, 48, 83, 147, 223, 146, 236, 167, 21, 138, 220, 244, 134, 85, 78, 24, 33, 64, 196, 196, 213, 198, 145, 138, 205, 231, 209, 78, 9,
			 50, 23, 223, 131, 255, 240, 14, 205, 246, 194, 25, 18, 117, 61, 233, 28, 184, 203, 43, 5, 170, 190, 22, 236, 182, 6, 221, 199, 179, 172, 99, 209,
			 95, 26, 101, 12, 152, 169, 201, 111, 73, 246, 211, 10, 69, 110, 122, 195, 42, 39, 140, 16, 32, 98, 226, 106, 227, 72, 197, 230, 243, 104, 167, 4,
			 153, 139, 239, 193, 127, 120, 135, 102, 123, 225, 12, 137, 186, 158, 116, 14, 220, 229, 149, 2, 85, 95, 11, 118, 91, 131, 238, 227, 89, 214, 177,
			 232, 47, 141, 50, 6, 204, 212, 228, 183, 36, 251, 105, 133, 34, 55, 189, 97, 149, 19, 70, 8, 16, 49, 113, 181, 113, 164, 98, 243, 121, 180, 83, 130,
			 204, 197, 247, 224, 63, 188, 67, 179, 189, 112, 134, 68, 93, 79, 58, 7, 238, 242, 74, 129, 170, 175, 5, 187, 173, 65, 247, 241, 44, 235, 88, 244, 151,
			 70, 25, 3, 102, 106, 242, 91, 146, 253, 180, 66, 145, 155, 222, 176, 202, 9, 35, 4, 136, 152, 184, 218, 56, 82, 177, 249, 60, 218, 41, 65, 230, 226, 123,
			 240, 31, 222, 161, 217, 94, 56, 67, 162, 174, 39, 157, 3, 119, 121, 165, 64, 213, 215, 130, 221, 214, 160, 251, 120, 150, 117, 44, 250, 75, 163, 140, 1,
			 51, 53, 249, 45, 201, 126, 90, 161, 200, 77, 111, 88, 229, 132, 17, 2, 68, 76, 92, 109, 28, 169, 216, 124, 30, 237, 148, 32, 115, 241, 61, 248, 15, 239,
			 208, 108, 47, 156, 33, 81, 215, 147, 206, 129, 187, 188, 82, 160, 234, 107, 193, 110, 107, 208, 125, 60, 203, 58, 22, 253, 165, 81, 198, 128, 153, 154,
			 252, 150, 100, 63, 173, 80, 228, 166, 55, 172, 114, 194, 8, 1, 34, 38, 174, 54, 142, 84, 108, 62, 143, 118, 74, 144, 185, 248, 30, 252, 135, 119, 104,
			 182, 23, 206, 144, 168, 235, 73, 231, 192, 93, 94, 41, 80, 245, 181, 96, 183, 53, 232, 62, 158, 101, 29, 139, 254, 210, 40, 99, 192, 76, 77, 126, 75,
			 178, 159, 86, 40, 114, 211, 27, 86, 57, 97, 132, 0, 17, 19, 87, 27, 71, 42, 54, 159, 71, 59, 37, 200, 92, 124, 15, 254, 195, 59, 52, 219, 11, 103, 72,
			 212, 245, 164, 115, 224, 46, 175, 20, 168, 250, 90, 176, 219, 26, 116, 31, 207, 178, 142, 69, 127, 105, 148, 49, 96, 166, 38, 191, 37, 217, 79, 43, 20,
			 185, 233, 13, 171, 156, 48, 66, 128, 136, 137, 171, 141, 35, 21, 155, 207, 163, 157, 18, 100, 46, 190, 7, 255, 225, 29], dtype=np.uint8)


def PN9_whitener_byte_sequence(n):
	PN9 = 0xff
	bytearr = [PN9,]
	for ii in range(n):
		if (ii%8) == 7:
			bytearr.append(PN9 & 0xff)
		PN9 = (PN9>>1) + ((((PN9 >> 5) & 1) ^ (PN9 & 1)) << 8)
	return np.array(bytearr, dtype=np.int32)
## PN9 SEQUENCE ==============================================================================================================================================================================
## PN9 SEQUENCE ==============================================================================================================================================================================




## CC1125 SETTINGS ===========================================================================================================================================================================
## CC1125 SETTINGS ===========================================================================================================================================================================
SRATE_M_153k6 = 0x0f7510
SRATE_E_153k6 = 0x0a
SRATE_M_76k8  = 0x0f7510
SRATE_E_76k8  = 0x09
SRATE_M_38k4  = 0x0f7510
SRATE_E_38k4  = 0x08
SRATE_M_19k2  = 0x0f7510
SRATE_E_19k2  = 0x07
SRATE_M_9k6   = 0x0f7510
SRATE_E_9k6   = 0x06
SRATE_M_4k8   = 0x0f7510
SRATE_E_4k8   = 0x05
SRATE_M_2k4   = 0x0f7510
SRATE_E_2k4   = 0x04
SRATE_M_1k2   = 0x0f7510
SRATE_E_1k2   = 0x03
SRATE_M_0k6   = 0x0f7510
SRATE_E_0k6   = 0x02
SRATE_M_0k3   = 0x0f7510
SRATE_E_0k3   = 0x01
SRATE_M_0k150 = 0x0fba88
SRATE_E_0k150 = 0x00


def CC1125_symbolrate_for_M_E(SRATE_M, SRATE_E):  # maxdf = 2.384185791015625
	assert 0 <= SRATE_E < 16
	assert 0 <= SRATE_M < (2**20)
	if SRATE_E == 0:
		return 40e6 * SRATE_M / 2**38
	return 40e6 * (2**20 + SRATE_M) * (2**SRATE_E) / (2**39)

def CC1125_M_E_for_symbolrate(symbolrate):
	assert 0 < symbolrate <= 4999997.615814209
	if symbolrate <= CC1125_symbolrate_for_M_E(SRATE_M=0x0fffff, SRATE_E=0):
		return int(symbolrate * (2**38) / 40e6), 0
	E = int(np.log2( symbolrate * 2**39 / 40e6 ) - 20)
	M = int((symbolrate * 2**39 / (40e6 * 2**E)) - 2**20)
	return M, E

#    (40e6 / 2**24) * (256 + DEV_M) * 2**DEV_E     	|| where DEV_M is int8 and DEV_E is int3
def CC1125_peak_deviation_for_M_E(DEV_M, DEV_E):
	assert 0 <= DEV_E <= (2**3 -1)
	assert 0 <= DEV_M <= (2**8 -1)
	if DEV_E == 0:
		return (40e6/(2**23)) * DEV_M
	return (40e6/(2**24)) * (256+DEV_M) * 2**DEV_E

def CC1125_DEV_M_E_for_peak_deviation(f_dev):
	if f_dev < 256 * 40e6 / (2**23):
		E = 0
		M = int(f_dev * 2**23 / 40e6)
		assert M < 256
		return M, E
	E = int(np.log( f_dev * 2**24 /(40e6 * 256) ) / np.log(2))
	M = int((f_dev * 2**24 / ((2**E) * 40e6)) - 256)
	assert E < 8
	assert M < 256
	return M, E
## CC1125 SETTINGS ===========================================================================================================================================================================
## CC1125 SETTINGS ===========================================================================================================================================================================






# SAMPLE GENERATION ==========================================================================================================================================================================
# SAMPLE GENERATION ==========================================================================================================================================================================
GAUSS_STD_PER_HALFPOINTS = 1 / ((np.log(2) * 2) ** 0.5)
@njit(cache=True)
def gauss_curve(std, x):
	a = 1/(std*np.sqrt(2*np.pi))
	return a * np.exp(-0.5 * ((x/std)**2))  # x = (x-mu)

@njit(cache=True)
def gauss_curve_sps(sps_f, BT, n_taps):
	std = sps_f * GAUSS_STD_PER_HALFPOINTS / (2*BT)
	x = np.linspace(-1.0, 1.0, n_taps) * (n_taps-1)
	curve = gauss_curve(std, x)
	return curve / np.sum(curve)

@njit(cache=True)
def sinc_curve(BT, sps_f, n_taps):
	tperT = np.linspace(-1.0, 1.0, n_taps) * (n_taps-1)
	pulse = np.sinc(tperT * BT / sps_f)
	return pulse / np.sum(pulse)

@njit(cache=True)
def make_squarewave(binary_symbols, sps_f, i_sample_of_sym0_f, nsamples, npad):
	assert np.all(np.isclose(binary_symbols, 1) + np.isclose(binary_symbols, -1))
	if nsamples < 0:
		nsamples = int(len(binary_symbols) * sps_f + i_sample_of_sym0_f)
	samples = np.zeros(nsamples)
	for i in range(nsamples):
		isym = int((i-i_sample_of_sym0_f) / sps_f)
		if isym < 0:
			continue
		if isym < len(binary_symbols):
			samples[i] = binary_symbols[isym]
	if npad > 0:
		pad = np.zeros(npad, dtype=np.float64)
		samples = np.concatenate( (pad, samples, pad) )
	return samples


@njit(cache=True, parallel=True)
def make_f_modulating_waveform(binary_symbols, sps_f, shaper_mode, shaper_BT_prod, shaper_n_taps):
	assert shaper_mode in (0,1)
	assert (shaper_BT_prod > 0) or (shaper_BT_prod == -1)
	if shaper_BT_prod > 0:
		if shaper_mode == 0:
			pulse = sinc_curve(BT=shaper_BT_prod, sps_f=sps_f, n_taps=shaper_n_taps)
		else:
			pulse = gauss_curve_sps(sps_f=sps_f, BT=shaper_BT_prod, n_taps=shaper_n_taps)
		assert len(pulse) == shaper_n_taps
	else:
		pulse = np.ones(1, dtype=np.float64)
	npulse = len(pulse)
	modulator0 = make_squarewave(binary_symbols=binary_symbols, sps_f=sps_f, i_sample_of_sym0_f=0.0, nsamples=-1, npad=len(pulse)//2)
	if npulse > 1:
		#modulator1 = np.correlate(modulator0, pulse)
		modulator1 = np.zeros(len(modulator0)-npulse+1, dtype=np.float64)
		for i in prange(len(modulator1)):
			modulator1[i] = np.sum(pulse * modulator0[i:i+npulse])

	else:
		modulator1 = modulator0
	modulator1 = modulator1 / np.max(np.abs(modulator1))
	return modulator1


@njit(cache=True)
def fm_mod(f_signal_offset, peak_deviation, modulator):
	assert np.min(modulator) >= -1.0
	assert np.max(modulator) <=  1.0
	assert abs(f_signal_offset) < 0.5
	nn = len(modulator)

	#cs_mod = np.cumsum(modulator)

	cs_mod = np.zeros(nn, dtype=np.float64)
	cs_mod[0] = modulator[0]
	for i in range(1,nn):
		cs_mod[i] = modulator[i] + cs_mod[i-1]

	signal = np.exp((2j*np.pi) * (f_signal_offset * np.arange(nn) + cs_mod * peak_deviation)) # TODO: make the frequency offset it's own exp-multiplication. Math would be cleaner.
	return signal


#@njit(cache=True)
def make_samples(sps_f, bitstring, f_offset, power, mod_index=0.5, shaper_mode=1, shaper_BT_prod=0.8, shaper_n_taps=301, n_silence_start=0, n_silence_end=0):
	assert abs(f_offset) < 0.5
	assert (shaper_BT_prod > 0) or (shaper_BT_prod == -1)
	# Apparently max deviation of CC1125 is about 155.9 kHz.          (40e6 / 2**24) * (256 + DEV_M) * 2**DEV_E     	|| where DEV_M is int8 and DEV_E is int3
	# 															 or   (40e6 / 2**23) * DEV_M  						|| if DEV_E = 0
	# peak_dev = mod_index / (2*symboltime).                          peak_dev_physical = peak_dev * sr. accords to CC1125 (CC112X/CC1175) User's guide on page 26.
	peak_dev	= mod_index / (sps_f*2.0)
	modulator 	= make_f_modulating_waveform(bitstring, sps_f, shaper_mode, shaper_BT_prod, shaper_n_taps)
	samples 	= fm_mod(f_offset, peak_dev, modulator)
	#print(np.average(np.abs(samples)))
	#samples 	= samples / np.average(np.abs(samples))
	samples 	= samples * (power**0.5)
	if (n_silence_start > 0) or (n_silence_end > 0):
		samples = np.concatenate( (np.zeros(n_silence_start, dtype=np.complex128), samples, np.zeros(n_silence_end, dtype=np.complex128)) )
	return samples


@njit(cache=True)
def bytes_to_bits(data):
	bits = np.zeros(len(data)*8, dtype=np.int64)
	for i in range(len(data)):
		for j in range(8):
			bits[i*8+j] = (data[i]>>j) & 1
	return bits


@njit(cache=True)
def ints_to_bits(int_arr, bits_per_int):
	bits = np.zeros(len(int_arr)*bits_per_int, dtype=np.int64)
	for i in range(len(int_arr)):
		for j in range(bits_per_int):
			bits[i*bits_per_int+j] = (int_arr[i]>>j) & 1
	return bits


def radionoise(n, sr, W_per_Hz):
	"""
	:param n: number of samples
	:param sr: samplerate
	:param W_per_Hz: spectral power density (Watts per Hertz)
	:return: n samples of normal distributed IQ noise
	To verify signal energy, and spectral power density:
		with df = sr/n
		sum((fft(samples)*df)**2) ≈ W_per_Hz * sr
		- Each fft-bin is (2*f_Nyquist) / n  Hertz wide.
		- fft bins represent amplitudes of constituent component frquencies.
	"""
	cc = (0.5*W_per_Hz*sr)**0.5
	return cc * (np.random.normal(0,1.0, n) + 1j*np.random.normal(0, 1.0, n))
# SAMPLE GENERATION ==========================================================================================================================================================================
# SAMPLE GENERATION ==========================================================================================================================================================================



def doppler_correction(f_received, f_original, f_at_target):
	c = 299792458.0
	#f_received = f_original * c/(c+v_src)
	v_src = c * (f_original/f_received - 1)
	f_send = f_at_target * c/(c-v_src)
	return f_send, v_src # v_src is the derivative of separating distance. (negative if satellite is approaching)




def pll_df_std0_polyfit(c_freq, c_limit):
	"""
	This estimates pll-df-std0: the standard deviation of the first derivative of the frequency correction term of a nco-pll with pure noise input.
	This term can be used as an exceedinly sensitive transmission detector, with the following rules:
		- start of a transmission is indicated by the standard deviation (when measured on a sliding window) falling below C * pll-df-std0.
		- end of a transmission is indicated by the standard deviation returning back to >= pll-df-std0.
	"""
	coeffs = [ -6.588834003638643e-10, -2.5411768833287384e-09, 4.981408860432077e-08, 1.868537663753007e-07,
			   -1.6876213332527186e-06, -6.0927590116583276e-06, 3.381339596106427e-05, 0.00011601629871765534,
			   -0.0004452159771682212, -0.001429960454287664, 0.0040467863424894085, 0.011952688145626354,
			   -0.025947547880166965, -0.06904834881024477, 0.11779091026188404, 0.2760086678268451,
			   -0.37435257361191565, -0.7542160313297891, 0.8113440477869422, 1.3790490653091885,
			   -1.1482083879741865, -1.6614950216935542, 0.9964447101954652, 1.4083888509528601,
			   -0.4914604337344185, -1.3722975909449475, 1.033787593289162, ]
	x = np.log10(c_freq/c_limit)
	y = 0
	for ip in range(27):
		y += coeffs[ip] * x**(27-(ip+1))
	y = 1.813*(x < np.log10(0.003)) + y*(np.log10(0.003) <= x)*(x < 3) + 0.0*(3 <= x)
	return y












