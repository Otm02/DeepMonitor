#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

// Sanity test to verify the build toolchain works.
TEST(Sanity, GoogleTestWorks) {
    EXPECT_EQ(1 + 1, 2);
}

RC_GTEST_PROP(Sanity, RapidCheckWorks, ()) {
    int a = *rc::gen::inRange(0, 100);
    int b = *rc::gen::inRange(0, 100);
    RC_ASSERT(a + b >= 0);
}
