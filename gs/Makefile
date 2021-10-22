

BUILD = build/gs
TEST1_BUILD = build/test

INCS =  -I. -I.. -I../src -I../src/tests
LIBS = -lzmq -lm
TEST1_LIBS = -lzmq -lm -pthread

CFLAGS =  -Wall -Wextra -g $(INCS)

#	../src/arq.c
#	../src/mac.c
#	../src/mux.c
#	../src/buf.c \

SRCS= \
	main.c \
	vc_interface.c \
	modem_suo.c \
	../src/elementbuffer.c \
	../src/fec.c \
	../src/hmac.c \
	../src/skylink.c \
	../src/ext/cifra/blockwise.c \
	../src/ext/cifra/hmac.c \
	../src/ext/cifra/sha256.c \
	../src/ext/gr-satellites/golay24.c \
	../src/ext/libfec/decode_rs_8.c \
	../src/ext/libfec/encode_rs_8.c \
	../src/ext/libfec/ccsds_tab.c



TEST1_SRCS= \
	../src/mac2.c \
	../src/skypacket.c \
	../src/skylink_array.c \
	../src/elementbuffer.c \
	../src/tests/testmain.c \
	../src/tests/elebuffer_tests.c \
	../src/tests/ring_test.c \
	../src/tests/tools/tools.c \
	../src/tests/crc/crc_algos.c



OBJS = $(addprefix $(BUILD)/,$(SRCS:.c=.o))

TEST1_OBJS = $(addprefix $(TEST1_BUILD)/,$(TEST1_SRCS:.c=.o))



buffertest: $(TEST1_OBJS)
	$(CC) -o $@ $(TEST1_OBJS) $(TEST1_LIBS)

$(TEST1_BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@





skylink: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@





clean:
	rm -r $(BUILD)
	rm -r $(TEST1_BUILD)