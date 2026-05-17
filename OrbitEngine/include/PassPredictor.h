#pragma once
#include "SGP4.h"
#include <vector>
#include <string>

namespace OrbitEngine {

struct GroundStation {
    double lat;    // degrees
    double lon;    // degrees
    double alt;    // km
    std::string name;
};

struct PassEvent {
    double aos_unix;       // Acquisition of Signal (unix timestamp)
    double los_unix;       // Loss of Signal (unix timestamp)
    double max_elev;       // degrees
    double max_elev_unix;  // time of max elevation
    double aos_az;         // azimuth at AOS (degrees)
    double los_az;         // azimuth at LOS (degrees)
};

struct AltAz {
    double elevation; // degrees
    double azimuth;   // degrees
    double range;     // km
};

class PassPredictor {
public:
    PassPredictor(const SGP4& sgp4, const GroundStation& gs);

    // Predict passes in [start_unix, start_unix + duration_sec]
    // min_elevation in degrees
    std::vector<PassEvent> predict(double start_unix,
                                   double duration_sec,
                                   double min_elevation = 5.0) const;

    // Compute elevation/azimuth at a specific time
    AltAz lookAngle(double unix_timestamp) const;

private:
    const SGP4&          m_sgp4;
    const GroundStation& m_gs;

    double elevation(double unix_timestamp) const;
};

} // namespace OrbitEngine
