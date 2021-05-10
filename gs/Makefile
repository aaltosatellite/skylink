

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
	../ext/cifra/blockwise.c \
	../ext/cifra/hmac.c \
	../ext/cifra/sha256.c \
	../ext/gr-satellites/golay24.c \
	../ext/libfec/decode_rs_8.c \
	../ext/libfec/encode_rs_8.c \
	../ext/libfec/ccsds_tab.c

OBJS = $(addprefix $(BUILD)/,$(SRCS:.c=.o))

skylink: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)


$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm $(OBJS)
