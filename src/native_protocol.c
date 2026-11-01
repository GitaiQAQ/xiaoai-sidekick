#include "native_protocol.h"
#include <string.h>

struct input { lx06_read_metadata read; void *context; };
struct cursor { const struct input *input; size_t offset, n; };
struct field { uint32_t number, wire; uint64_t scalar; struct cursor bytes; };

static int varint(struct cursor *c, uint64_t *v) {
    unsigned i;
    *v = 0;
    for (i = 0; i < 10; ++i) {
        uint8_t b;
        if (!c->n) return -1;
        if (c->input->read(c->input->context,c->offset,&b)) return -1;
        ++c->offset; --c->n;
        if (i == 9 && (b & 0xfe)) return -1;
        *v |= (uint64_t)(b & 0x7f) << (i * 7);
        if (!(b & 0x80)) return 0;
    }
    return -1;
}

static int next(struct cursor *c, struct field *f) {
    uint64_t key, length;
    size_t skip;
    memset(f, 0, sizeof(*f));
    if (!c->n) return 0;
    if (varint(c, &key) || !(key >> 3) || (key >> 3) > 0x1fffffff) return -1;
    f->number = (uint32_t)(key >> 3); f->wire = key & 7;
    switch (f->wire) {
        case 0: return varint(c, &f->scalar) ? -1 : 1;
        case 1: skip = 8; break;
        case 2:
            if (varint(c, &length) || length > c->n) return -1;
            f->bytes = *c; f->bytes.n = (size_t)length;
            skip = (size_t)length; break;
        case 5: skip = 4; break;
        default: return -1; /* Groups are absent from the recovered schema. */
    }
    if (skip > c->n) return -1;
    c->offset += skip; c->n -= skip;
    return 1;
}

static int stream_metadata(struct cursor c, struct lx06_packet_metadata *m) {
    struct field f;
    int r, seen_type = 0, seen_stream = 0;
    while ((r = next(&c, &f)) > 0) {
        if (f.number == 1) {
            if (f.wire != 0 || f.scalar > UINT32_MAX || seen_type++) return -1;
            m->stream_type = (uint32_t)f.scalar;
        } else if (f.number == 2) {
            if (f.wire != 2 || seen_stream++) return -1;
            m->audio_bytes = f.bytes.n;
        }
    }
    if (r < 0 || !seen_type || !seen_stream) return -1;
    m->has_stream = 1;
    return 0;
}

static int child_metadata(struct cursor c, struct lx06_packet_metadata *m) {
    struct field f;
    int r, seen_type = 0;
    while ((r = next(&c, &f)) > 0) {
        if (f.number == 1) {
            if (f.wire != 0 || f.scalar > UINT32_MAX || seen_type++) return -1;
            m->type = (uint32_t)f.scalar;
        } else if (m->direction == 0 && f.number == 4) {
            if (f.wire != 2 || m->has_stream || stream_metadata(f.bytes, m)) return -1;
        }
    }
    if (r < 0 || !seen_type) return -1;
    if (m->direction == 0 && ((m->type == 3) != (m->has_stream != 0))) return -1;
    return 0;
}

int lx06_packet_metadata_reader(lx06_read_metadata read, void *context, size_t size,
                                struct lx06_packet_metadata *out) {
    struct input input = {read,context};
    struct cursor c = {&input,0,size}, up = {0}, down = {0};
    struct lx06_packet_metadata m;
    struct field f;
    int r, seen_type = 0, seen_up = 0, seen_down = 0;
    if (!read || !out || !size || size > 65536) return -1;
    memset(&m, 0, sizeof(m));
    while ((r = next(&c, &f)) > 0) {
        if (f.number == 1) {
            if (f.wire != 0 || f.scalar > 1 || seen_type++) return -1;
            m.direction = (uint32_t)f.scalar;
        } else if (f.number == 2 || f.number == 3) {
            if (f.wire != 2) return -1;
            if (f.number == 2) { if (seen_up++) return -1; up = f.bytes; }
            else { if (seen_down++) return -1; down = f.bytes; }
        }
    }
    if (r < 0 || !seen_type || seen_up + seen_down != 1 ||
        (m.direction == 0 && !seen_up) || (m.direction == 1 && !seen_down)) return -1;
    if (child_metadata(m.direction == 0 ? up : down, &m)) return -1;
    *out = m;
    return 0;
}

static int memory_byte(void *context, size_t offset, uint8_t *byte) {
    *byte = ((const uint8_t *)context)[offset];
    return 0;
}

int lx06_packet_metadata(const uint8_t *packet, size_t size,
                         struct lx06_packet_metadata *out) {
    if (!packet) return -1;
    return lx06_packet_metadata_reader(memory_byte,(void*)packet,size,out);
}

const char *lx06_packet_type_name(uint32_t direction, uint32_t type) {
    static const char *up[] = {"register", "prepare", "cancel", "stream",
        "wakeup_latency", "voip", "tts_finish", "multi_channel_upload"};
    static const char *down[] = {"register_response", "prepare_response", "stop_capture",
        "expect_speech", "truncation", "dialog_finish", "asr_timeout", "asr_partial",
        "disable_wakeup", "enable_wakeup", "disconnected"};
    if (direction == 0 && type < sizeof(up)/sizeof(up[0])) return up[type];
    if (direction == 1 && type < sizeof(down)/sizeof(down[0])) return down[type];
    return "unknown";
}
