#include "flash_buffer.h"
#include "board.h"
#include <cstring>

static const size_t MAX_PAYLOAD_LEN = 512;
static const size_t MAX_ENTRIES = 64;

#ifdef HOST_TEST

namespace {
    char s_queue[MAX_ENTRIES][MAX_PAYLOAD_LEN];
    size_t s_head = 0;
    size_t s_tail = 0;
    size_t s_count = 0;
}

namespace flash_buffer {

bool init() { return true; }

bool enqueue(const char* json) {
    if (!json || s_count >= MAX_ENTRIES) {
        return false;
    }
    strncpy(s_queue[s_tail], json, MAX_PAYLOAD_LEN - 1);
    s_queue[s_tail][MAX_PAYLOAD_LEN - 1] = '\0';
    s_tail = (s_tail + 1) % MAX_ENTRIES;
    s_count++;
    return true;
}

int drain(SendFn send_fn) {
    if (!send_fn) return 0;
    int sent = 0;
    while (s_count > 0) {
        if (send_fn(s_queue[s_head])) {
            sent++;
        } else {
            break;
        }
        s_head = (s_head + 1) % MAX_ENTRIES;
        s_count--;
    }
    return sent;
}

size_t pending_count() { return s_count; }

void clear() {
    s_head = s_tail = s_count = 0;
}

namespace mock {
void reset() { clear(); }
}

} // namespace flash_buffer

#elif HAS_FLASH_BUFFER

#include <Arduino.h>
#include <SPI.h>

// W25Q32 minimal SPI flash — sector-based ring buffer in first 256KB
namespace {
    static const uint32_t FLASH_BASE = 0x000000;
    static const uint32_t SECTOR_SIZE = 4096;
    static const uint8_t W25_CMD_READ = 0x03;
    static const uint8_t W25_CMD_PAGE_PROG = 0x02;
    static const uint8_t W25_CMD_SECTOR_ERASE = 0x20;
    static const uint8_t W25_CMD_WRITE_ENABLE = 0x06;
    static const uint8_t W25_CMD_READ_STATUS = 0x05;

    uint8_t s_meta[8];  // head_idx, tail_idx, count (each uint16), reserved

    void cs_low()  { digitalWrite(FLASH_CS_PIN, LOW); }
    void cs_high() { digitalWrite(FLASH_CS_PIN, HIGH); }

    void w25_wait_ready() {
        cs_low();
        SPI.transfer(W25_CMD_READ_STATUS);
        while (SPI.transfer(0) & 0x01) {}
        cs_high();
    }

    void w25_write_enable() {
        cs_low();
        SPI.transfer(W25_CMD_WRITE_ENABLE);
        cs_high();
    }

    void w25_read(uint32_t addr, uint8_t* buf, size_t len) {
        cs_low();
        SPI.transfer(W25_CMD_READ);
        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);
        for (size_t i = 0; i < len; i++) {
            buf[i] = SPI.transfer(0);
        }
        cs_high();
    }

    void w25_page_program(uint32_t addr, const uint8_t* data, size_t len) {
        w25_write_enable();
        cs_low();
        SPI.transfer(W25_CMD_PAGE_PROG);
        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);
        for (size_t i = 0; i < len; i++) {
            SPI.transfer(data[i]);
        }
        cs_high();
        w25_wait_ready();
    }

    void load_meta() {
        w25_read(FLASH_BASE, s_meta, sizeof(s_meta));
    }

    void save_meta() {
        w25_write_enable();
        cs_low();
        SPI.transfer(W25_CMD_SECTOR_ERASE);
        SPI.transfer(0);
        SPI.transfer(0);
        SPI.transfer(0);
        cs_high();
        w25_wait_ready();
        w25_page_program(FLASH_BASE, s_meta, sizeof(s_meta));
    }

    uint32_t entry_addr(uint16_t idx) {
        return FLASH_BASE + SECTOR_SIZE + static_cast<uint32_t>(idx) * MAX_PAYLOAD_LEN;
    }
}

namespace flash_buffer {

bool init() {
    pinMode(FLASH_CS_PIN, OUTPUT);
    cs_high();
    SPI.begin(FLASH_SPI_SCK, FLASH_SPI_MISO, FLASH_SPI_MOSI, FLASH_CS_PIN);
    load_meta();
    return true;
}

bool enqueue(const char* json) {
    if (!json) return false;
    uint16_t count = static_cast<uint16_t>(s_meta[4]) | (static_cast<uint16_t>(s_meta[5]) << 8);
    if (count >= MAX_ENTRIES) return false;

    uint16_t tail = static_cast<uint16_t>(s_meta[2]) | (static_cast<uint16_t>(s_meta[3]) << 8);
    uint8_t buf[MAX_PAYLOAD_LEN];
    size_t len = strlen(json);
    if (len >= MAX_PAYLOAD_LEN) len = MAX_PAYLOAD_LEN - 1;
    memcpy(buf, json, len);
    buf[len] = '\0';
    for (size_t i = len + 1; i < MAX_PAYLOAD_LEN; i++) {
        buf[i] = 0;
    }

    w25_page_program(entry_addr(tail), buf, MAX_PAYLOAD_LEN);
    tail = (tail + 1) % MAX_ENTRIES;
    count++;

    s_meta[2] = tail & 0xFF;
    s_meta[3] = (tail >> 8) & 0xFF;
    s_meta[4] = count & 0xFF;
    s_meta[5] = (count >> 8) & 0xFF;
    save_meta();
    return true;
}

int drain(SendFn send_fn) {
    if (!send_fn) return 0;
    uint16_t count = static_cast<uint16_t>(s_meta[4]) | (static_cast<uint16_t>(s_meta[5]) << 8);
    uint16_t head = static_cast<uint16_t>(s_meta[0]) | (static_cast<uint16_t>(s_meta[1]) << 8);
    int sent = 0;
    char buf[MAX_PAYLOAD_LEN];

    while (count > 0) {
        w25_read(entry_addr(head), reinterpret_cast<uint8_t*>(buf), MAX_PAYLOAD_LEN);
        if (!send_fn(buf)) {
            break;
        }
        sent++;
        head = (head + 1) % MAX_ENTRIES;
        count--;
    }

    s_meta[0] = head & 0xFF;
    s_meta[1] = (head >> 8) & 0xFF;
    s_meta[4] = count & 0xFF;
    s_meta[5] = (count >> 8) & 0xFF;
    if (sent > 0) {
        save_meta();
    }
    return sent;
}

size_t pending_count() {
    return static_cast<uint16_t>(s_meta[4]) | (static_cast<uint16_t>(s_meta[5]) << 8);
}

void clear() {
    memset(s_meta, 0, sizeof(s_meta));
    save_meta();
}

} // namespace flash_buffer

#else

// M5 builds — no external flash; in-RAM fallback for failed uploads (single cycle only)
namespace {
    char s_pending[MAX_PAYLOAD_LEN];
    bool s_has_pending = false;
}

namespace flash_buffer {

bool init() { return true; }

bool enqueue(const char* json) {
    if (!json) return false;
    strncpy(s_pending, json, MAX_PAYLOAD_LEN - 1);
    s_pending[MAX_PAYLOAD_LEN - 1] = '\0';
    s_has_pending = true;
    return true;
}

int drain(SendFn send_fn) {
    if (!send_fn || !s_has_pending) return 0;
    if (send_fn(s_pending)) {
        s_has_pending = false;
        return 1;
    }
    return 0;
}

size_t pending_count() { return s_has_pending ? 1 : 0; }

void clear() { s_has_pending = false; }

} // namespace flash_buffer

#endif // HAS_FLASH_BUFFER
