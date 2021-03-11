
struct suoframe {
	uint32_t id, flags;
	uint64_t time;
	uint32_t metadata[11];
	uint32_t len;
	uint8_t data[RADIOFRAME_MAXLEN];
};

struct suotiming {
	uint32_t id, flags;
	uint64_t time;
};
