#include <gtest/gtest.h>
#include "flash_buffer.h"
#include <cstring>

namespace {
int g_send_count = 0;
char g_last_sent[512];

bool mock_send(const char* json) {
    strncpy(g_last_sent, json, sizeof(g_last_sent) - 1);
    g_last_sent[sizeof(g_last_sent) - 1] = '\0';
    g_send_count++;
    return true;
}

bool mock_send_fail(const char* /*json*/) {
    return false;
}
} // namespace

class FlashBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        flash_buffer::mock::reset();
        g_send_count = 0;
        g_last_sent[0] = '\0';
        flash_buffer::init();
    }
};

TEST_F(FlashBufferTest, EnqueueAndDrainPreservesOrder) {
    ASSERT_TRUE(flash_buffer::enqueue("{\"type\":\"heartbeat\",\"seq\":1}"));
    ASSERT_TRUE(flash_buffer::enqueue("{\"type\":\"heartbeat\",\"seq\":2}"));
    EXPECT_EQ(flash_buffer::pending_count(), 2u);

    int sent = flash_buffer::drain(mock_send);
    EXPECT_EQ(sent, 2);
    EXPECT_EQ(g_send_count, 2);
    EXPECT_EQ(flash_buffer::pending_count(), 0u);
    EXPECT_STREQ(g_last_sent, "{\"type\":\"heartbeat\",\"seq\":2}");
}

TEST_F(FlashBufferTest, DrainStopsOnFirstFailure) {
    flash_buffer::enqueue("{\"a\":1}");
    flash_buffer::enqueue("{\"b\":2}");

    int sent = flash_buffer::drain(mock_send_fail);
    EXPECT_EQ(sent, 0);
    EXPECT_EQ(flash_buffer::pending_count(), 2u);
}

TEST_F(FlashBufferTest, ClearEmptiesQueue) {
    flash_buffer::enqueue("{\"x\":1}");
    flash_buffer::clear();
    EXPECT_EQ(flash_buffer::pending_count(), 0u);
}
