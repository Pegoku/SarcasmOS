#include "swd_rtt.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "hardware/sync.h"

namespace {

constexpr uint32_t kBufferSize = 256;
char terminal_buffer[kBufferSize];
const char terminal_name[] = "Terminal";

struct RttBuffer {
    const char *name;
    char *buffer;
    uint32_t size;
    volatile uint32_t write_offset;
    volatile uint32_t read_offset;
    uint32_t flags;
};

struct RttControlBlock {
    char id[16];
    uint32_t max_up_buffers;
    uint32_t max_down_buffers;
    RttBuffer up[1];
    RttBuffer down[1];
};

static_assert(sizeof(RttBuffer) == 24, "RTT requires 32-bit pointers");
static_assert(sizeof(RttControlBlock) == 72, "unexpected RTT control block layout");

}  // namespace

extern "C" {

__attribute__((used, aligned(4))) RttControlBlock swd_rtt_control_block = {
    "SEGGER RTT",
    1,
    0,
    {{terminal_name, terminal_buffer, kBufferSize, 0, 0, 0}},
    {{nullptr, nullptr, 0, 0, 0, 0}},
};

}  // extern "C"

void swd_rtt_write(const char *text) {
    RttBuffer &output = swd_rtt_control_block.up[0];
    const uint32_t length = static_cast<uint32_t>(strlen(text));
    const uint32_t write_offset = output.write_offset;
    const uint32_t read_offset = output.read_offset;
    const uint32_t free_space =
        write_offset >= read_offset
            ? output.size - 1 - (write_offset - read_offset)
            : read_offset - write_offset - 1;
    if (length > free_space) return;

    uint32_t next_offset = write_offset;
    while (*text != '\0') {
        output.buffer[next_offset++] = *text++;
        if (next_offset == output.size) next_offset = 0;
    }
    __dmb();
    output.write_offset = next_offset;
}

void swd_rtt_printf(const char *format, ...) {
    char line[128];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    swd_rtt_write(line);
}
