

BUILD = build/gs

INCS =  -I. -I.. -I../src
LIBS = -lzmq

CFLAGS =  -Wall -Wextra -g $(INCS)

#	../src/arq.c
#	../src/mac.c
#	../src/mux.c

SRCS= \
	main.c \
	vc_interface.c \
	modem_suo.c \
	../src/buf.c \
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

OBJS = $(addprefix $(BUILD)/,$(SRCS:.c=.o))

skylink: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)


$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm $(OBJS)
