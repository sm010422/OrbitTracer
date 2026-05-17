#include "PassPredictor.h"
#include "SGP4.h"
#include <cmath>
#include <algorithm>

namespace OrbitEngine {

using namespace Constants;

PassPredictor::PassPredictor(const SGP4& sgp4, const GroundStation& gs)
    : m_sgp4(sgp4), m_gs(gs) {}

// Compute ECEF position of ground station using WGS84
static void gsECEF(double lat_deg, double lon_deg, double alt_km,
                   double& x, double& y, double& z) {
    double lat  = lat_deg * DEG2RAD;
    double lon  = lon_deg * DEG2RAD;
    double clat = std::cos(lat), slat = std::sin(lat);
    double clon = std::cos(lon), slon = std::sin(lon);
    double a    = RE;
    double e2   = 2.0 * F - F * F;
    double N    = a / std::sqrt(1.0 - e2 * slat * slat);
    x = (N + alt_km) * clat * clon;
    y = (N + alt_km) * clat * slon;
    z = (N * (1.0 - e2) + alt_km) * slat;
}

AltAz PassPredictor::lookAngle(double unix_timestamp) const {
    EciState eci = m_sgp4.propagateUnix(unix_timestamp);
    if (!eci.valid) return {-90.0, 0.0, 0.0};

    double gmst = computeGMST(unix_timestamp);

    // Satellite ECI -> ECEF
    double cosG = std::cos(gmst), sinG = std::sin(gmst);
    double sx   =  eci.pos[0] * cosG + eci.pos[1] * sinG;
    double sy   = -eci.pos[0] * sinG + eci.pos[1] * cosG;
    double sz   =  eci.pos[2];

    // Ground station ECEF
    double gx, gy, gz;
    gsECEF(m_gs.lat, m_gs.lon, m_gs.alt, gx, gy, gz);

    // Range vector (satellite - GS) in ECEF
    double rx = sx - gx, ry = sy - gy, rz = sz - gz;
    double range = std::sqrt(rx * rx + ry * ry + rz * rz);

    // Convert range vector to South-East-Zenith (SEZ) topocentric frame
    double lat  = m_gs.lat * DEG2RAD;
    double lon  = m_gs.lon * DEG2RAD;
    double clat = std::cos(lat), slat = std::sin(lat);
    double clon = std::cos(lon), slon = std::sin(lon);

    // SEZ transformation:
    //   S (south)  = slat*clon*x + slat*slon*y - clat*z
    //   E (east)   = -slon*x     + clon*y
    //   Z (zenith) = clat*clon*x + clat*slon*y + slat*z
    double rS = slat * clon * rx + slat * slon * ry - clat * rz;
    double rE = -slon * rx + clon * ry;
    double rZ = clat * clon * rx + clat * slon * ry + slat * rz;

    double elevation = std::asin(rZ / range) * RAD2DEG;
    double azimuth   = std::atan2(-rE, rS) * RAD2DEG; // measured N through E
    if (azimuth < 0.0) azimuth += 360.0;

    return {elevation, azimuth, range};
}

double PassPredictor::elevation(double unix_timestamp) const {
    return lookAngle(unix_timestamp).elevation;
}

std::vector<PassEvent> PassPredictor::predict(double start_unix,
                                               double duration_sec,
                                               double min_elevation) const {
    std::vector<PassEvent> passes;

    const double coarseStep = 15.0; // seconds — coarse scan
    double t   = start_unix;
    double end = start_unix + duration_sec;

    bool      above       = false;
    PassEvent current{};
    double    maxElev     = -90.0;
    double    maxElevTime = start_unix;

    while (t <= end) {
        double elev = elevation(t);

        if (!above && elev >= min_elevation) {
            // Acquisition of Signal — refine with bisection
            double t0 = t - coarseStep;
            double t1 = t;
            if (t0 < start_unix) t0 = start_unix;
            for (int iter = 0; iter < 25; ++iter) {
                double tm = 0.5 * (t0 + t1);
                if (elevation(tm) >= min_elevation) t1 = tm;
                else                                t0 = tm;
            }
            current.aos_unix = 0.5 * (t0 + t1);
            current.aos_az   = lookAngle(current.aos_unix).azimuth;
            maxElev          = elev;
            maxElevTime      = t;
            above            = true;
        }

        if (above) {
            if (elev > maxElev) {
                maxElev     = elev;
                maxElevTime = t;
            }

            if (elev < min_elevation || t >= end) {
                // Loss of Signal — refine with bisection
                double t0 = t - coarseStep;
                double t1 = t;
                if (t0 < current.aos_unix) t0 = current.aos_unix;
                for (int iter = 0; iter < 25; ++iter) {
                    double tm = 0.5 * (t0 + t1);
                    if (elevation(tm) >= min_elevation) t0 = tm;
                    else                                t1 = tm;
                }
                current.los_unix      = 0.5 * (t0 + t1);
                current.los_az        = lookAngle(current.los_unix).azimuth;
                current.max_elev      = maxElev;
                current.max_elev_unix = maxElevTime;
                passes.push_back(current);
                above    = false;
                maxElev  = -90.0;
                current  = PassEvent{};
            }
        }

        t += coarseStep;
    }

    return passes;
}

} // namespace OrbitEngine
