#ifndef LX06_NATIVE_PROTOCOL_H
#define LX06_NATIVE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

/* Metadata only. No audio, transcript, dialog identifier, or vendor is copied. */
struct lx06_packet_metadata {
    uint32_t direction;
    uint32_t type;
    uint32_t stream_type;
    size_t audio_bytes;
    unsigned has_stream;
};

/* Complete native speech datagram, <= 64 KiB. Zero on success. */
int lx06_packet_metadata(const uint8_t *packet, size_t size,
                         struct lx06_packet_metadata *out);
typedef int (*lx06_read_metadata)(void *context, size_t offset, uint8_t *byte);
/* Reads only protobuf keys, lengths and scalar metadata; skips byte payloads. */
int lx06_packet_metadata_reader(lx06_read_metadata read, void *context, size_t size,
                                struct lx06_packet_metadata *out);
const char *lx06_packet_type_name(uint32_t direction, uint32_t type);
#endif
