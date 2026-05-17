#include "OrbitEngineAPI.h"
#include "SGP4.h"
#include "TLEParser.h"
#include "PassPredictor.h"
#include <unordered_map>
#include <mutex>
#include <memory>
#include <cstring>
#include <algorithm>

namespace {

struct SatEntry {
    std::shared_ptr<OrbitEngine::SGP4> sgp4;
};

std::unordered_map<int, SatEntry> g_sats;
std::mutex g_mutex;
int g_nextHandle = 1;

} // anonymous namespace

extern "C" {

int orbit_init(const char* name, const char* line1, const char* line2) {
    try {
        auto tle  = OrbitEngine::TLEParser::parseTLE(name ? name : "", line1, line2);
        auto sgp4 = std::make_shared<OrbitEngine::SGP4>(tle);
        std::lock_guard<std::mutex> lock(g_mutex);
        int h = g_nextHandle++;
        g_sats[h] = {sgp4};
        return h;
    } catch (...) {
        return -1;
    }
}

void orbit_free(int handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_sats.erase(handle);
}

int orbit_get_position(int handle, double unix_ts, GeoPositionC* out) {
    if (!out) return -1;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sats.find(handle);
    if (it == g_sats.end()) return -1;
    auto state = it->second.sgp4->propagateUnix(unix_ts);
    if (!state.valid) return -1;
    auto geo = OrbitEngine::eciToGeodetic(state, unix_ts);
    out->lat = geo.lat;
    out->lon = geo.lon;
    out->alt = geo.alt;
    return 0;
}

int orbit_get_look_angle(int handle, double unix_ts,
                          double gs_lat, double gs_lon, double gs_alt,
                          AltAzC* out) {
    if (!out) return -1;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sats.find(handle);
    if (it == g_sats.end()) return -1;

    OrbitEngine::GroundStation gs;
    gs.lat = gs_lat;
    gs.lon = gs_lon;
    gs.alt = gs_alt;
    OrbitEngine::PassPredictor pred(*it->second.sgp4, gs);
    auto aa = pred.lookAngle(unix_ts);
    out->elevation = aa.elevation;
    out->azimuth   = aa.azimuth;
    out->range_km  = aa.range;
    return 0;
}

int orbit_predict_passes(int handle,
                          double gs_lat, double gs_lon, double gs_alt,
                          double start_unix, double duration_sec,
                          double min_elev_deg,
                          PassEventC* events, int max_passes) {
    if (!events || max_passes <= 0) return -1;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sats.find(handle);
    if (it == g_sats.end()) return -1;

    OrbitEngine::GroundStation gs;
    gs.lat = gs_lat;
    gs.lon = gs_lon;
    gs.alt = gs_alt;
    OrbitEngine::PassPredictor pred(*it->second.sgp4, gs);
    auto passes = pred.predict(start_unix, duration_sec, min_elev_deg);

    int count = std::min((int)passes.size(), max_passes);
    for (int i = 0; i < count; ++i) {
        events[i].aos_unix      = passes[i].aos_unix;
        events[i].los_unix      = passes[i].los_unix;
        events[i].max_elev      = passes[i].max_elev;
        events[i].max_elev_unix = passes[i].max_elev_unix;
        events[i].aos_az        = passes[i].aos_az;
        events[i].los_az        = passes[i].los_az;
    }
    return count;
}

int orbit_load_tle_file(const char* filepath, int* handles, int max_handles) {
    if (!filepath || !handles || max_handles <= 0) return -1;
    try {
        auto tles  = OrbitEngine::TLEParser::parseFile(filepath);
        int  count = std::min((int)tles.size(), max_handles);
        for (int i = 0; i < count; ++i) {
            auto sgp4 = std::make_shared<OrbitEngine::SGP4>(tles[i]);
            std::lock_guard<std::mutex> lock(g_mutex);
            int h = g_nextHandle++;
            g_sats[h] = {sgp4};
            handles[i] = h;
        }
        return count;
    } catch (...) {
        return -1;
    }
}

void orbit_get_name(int handle, char* buf, int buf_len) {
    if (!buf || buf_len <= 0) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sats.find(handle);
    if (it == g_sats.end()) { buf[0] = '\0'; return; }
    const std::string& name = it->second.sgp4->tle().name;
    std::strncpy(buf, name.c_str(), static_cast<size_t>(buf_len) - 1);
    buf[buf_len - 1] = '\0';
}

} // extern "C"
