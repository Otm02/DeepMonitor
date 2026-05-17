#ifndef FLASH_BUFFER_H
#define FLASH_BUFFER_H

#include <cstddef>
#include <cstdint>

namespace flash_buffer {

// Callback for draining buffered payloads. Return true on successful send.
using SendFn = bool (*)(const char* json);

bool init();
bool enqueue(const char* json);
// Drain all buffered entries; returns number successfully sent.
int drain(SendFn send_fn);
size_t pending_count();
void clear();

#ifdef HOST_TEST
namespace mock {
    void reset();
}
#endif

} // namespace flash_buffer

#endif // FLASH_BUFFER_H
