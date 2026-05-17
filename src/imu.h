#ifndef IMU_H
#define IMU_H

#include "config.h"

namespace imu {
    bool init();
    void sample(float vib_out[config::SAMPLE_COUNT], float temp_out[config::SAMPLE_COUNT]);

#ifdef HOST_TEST
    namespace mock {
        void set_should_fail(bool fail);
        void reset();
    }
#endif
}

#endif // IMU_H
