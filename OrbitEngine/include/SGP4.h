#pragma once
#include <cmath>
#include <string>

namespace OrbitEngine {

// Physical and mathematical constants
namespace Constants {
    constexpr double PI          = 3.14159265358979323846;
    constexpr double TWO_PI      = 2.0 * PI;
    constexpr double DEG2RAD     = PI / 180.0;
    constexpr double RAD2DEG     = 180.0 / PI;
    constexpr double MU          = 398600.4418;   // km^3/s^2
    constexpr double RE          = 6378.137;      // km (WGS84)
    constexpr double F           = 1.0 / 298.257223563; // flattening
    constexpr double J2          = 1.08262998905e-3;
    constexpr double J4          = -1.65597050e-6;
    constexpr double XKE         = 0.0743669161;  // sqrt(mu) in ER^3/2/min
    constexpr double XKMPER      = 6378.137;
    constexpr double AE          = 1.0;           // Earth radii
    constexpr double MIN_PER_DAY = 1440.0;
    constexpr double SEC_PER_DAY = 86400.0;
}

struct TLEData {
    std::string name;
    std::string line1;
    std::string line2;
    int         satnum;
    double      epoch;       // Julian date
    double      bstar;       // drag term (1/ER)
    double      inclo;       // inclination (rad)
    double      nodeo;       // RAAN (rad)
    double      ecco;        // eccentricity
    double      argpo;       // arg of perigee (rad)
    double      mo;          // mean anomaly (rad)
    double      no_kozai;    // mean motion (rad/min)
    double      ndot;        // first deriv mean motion (rad/min^2)
    double      nddot;       // second deriv mean motion (rad/min^3)
};

struct EciState {
    double pos[3]; // km
    double vel[3]; // km/s
    bool   valid;
};

struct GeoPosition {
    double lat;  // degrees
    double lon;  // degrees
    double alt;  // km
};

// SGP4 initialization data (computed once from TLE)
struct SGP4Init {
    TLEData tle;
    // Secular rates
    double mdot;
    double argpdot;
    double nodedot;
    double xlamo;
    // Drag / atmospheric
    double cc1, cc4, cc5;
    double d2, d3, d4;
    double eta;
    double omgcof;
    double sinmao;
    double t2cof, t3cof, t4cof, t5cof;
    double x1mth2, x7thm1;
    double delmo;
    double aycof, xlcof;
    double con41;
    bool   isimp;
};

class SGP4 {
public:
    explicit SGP4(const TLEData& tle);

    // Propagate to tsince minutes since epoch
    // Returns ECI position (km) and velocity (km/s)
    EciState propagate(double tsince_min) const;

    // Convenience: propagate to unix timestamp
    EciState propagateUnix(double unix_timestamp) const;

    double epochUnix() const { return m_epochUnix; }
    const TLEData& tle() const { return m_init.tle; }

private:
    SGP4Init m_init;
    double   m_epochUnix;

    void initialize(const TLEData& tle);
};

// Coordinate conversion utilities
GeoPosition eciToGeo(const EciState& eci, double gmst_rad);
double       computeGMST(double unix_timestamp);
GeoPosition  eciToGeodetic(const EciState& eci, double unix_timestamp);

} // namespace OrbitEngine
