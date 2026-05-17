#include "nvs.h"
#include "config.h"
#include <cstring>

namespace persist {

bool should_save_time_based(uint32_t duty_cycle_count, uint32_t last_nvs_save_cycle) {
    return (duty_cycle_count - last_nvs_save_cycle) >= static_cast<uint32_t>(config::NVS_SAVE_INTERVAL);
}

bool should_save_weight_shift(const ml::Weights& current, const ml::Weights& saved) {
    return ml::compute_weight_shift(current, saved) >= config::WEIGHT_SHIFT_THRESHOLD;
}

#ifdef HOST_TEST

static bool mock_has_data = false;
static ml::Weights mock_weights;
static normalizer::Stats mock_stats;

bool save(const ml::Weights& weights, const normalizer::Stats& stats) {
    memcpy(&mock_weights, &weights, sizeof(ml::Weights));
    memcpy(&mock_stats, &stats, sizeof(normalizer::Stats));
    mock_has_data = true;
    return true;
}

bool load(ml::Weights& weights, normalizer::Stats& stats) {
    if (!mock_has_data) return false;
    memcpy(&weights, &mock_weights, sizeof(ml::Weights));
    memcpy(&stats, &mock_stats, sizeof(normalizer::Stats));
    return true;
}

bool has_valid_data() {
    return mock_has_data;
}

void reset_mock() {
    mock_has_data = false;
    memset(&mock_weights, 0, sizeof(mock_weights));
    memset(&mock_stats, 0, sizeof(mock_stats));
}

#else

} // namespace persist — close before Arduino/Preferences (must not nest ESP-IDF headers)

#include <Preferences.h>

namespace persist {

static const char* PERSIST_NAMESPACE = "pdm_sensor";
static const char* KEY_WEIGHTS = "ml_weights";
static const char* KEY_STATS = "norm_stats";

bool save(const ml::Weights& weights, const normalizer::Stats& stats) {
    Preferences prefs;
    if (!prefs.begin(PERSIST_NAMESPACE, false)) return false;

    size_t w_written = prefs.putBytes(KEY_WEIGHTS, &weights, sizeof(ml::Weights));
    size_t s_written = prefs.putBytes(KEY_STATS, &stats, sizeof(normalizer::Stats));
    prefs.end();

    return (w_written == sizeof(ml::Weights)) && (s_written == sizeof(normalizer::Stats));
}

bool load(ml::Weights& weights, normalizer::Stats& stats) {
    Preferences prefs;
    if (!prefs.begin(PERSIST_NAMESPACE, true)) return false;

    size_t w_read = prefs.getBytes(KEY_WEIGHTS, &weights, sizeof(ml::Weights));
    size_t s_read = prefs.getBytes(KEY_STATS, &stats, sizeof(normalizer::Stats));
    prefs.end();

    return (w_read == sizeof(ml::Weights)) && (s_read == sizeof(normalizer::Stats));
}

bool has_valid_data() {
    Preferences prefs;
    if (!prefs.begin(PERSIST_NAMESPACE, true)) return false;

    size_t w_size = prefs.getBytesLength(KEY_WEIGHTS);
    size_t s_size = prefs.getBytesLength(KEY_STATS);
    prefs.end();

    return (w_size == sizeof(ml::Weights)) && (s_size == sizeof(normalizer::Stats));
}

#endif // HOST_TEST

} // namespace persist
