#include "SGP4.h"
#include <cmath>
#include <stdexcept>

namespace OrbitEngine {

using namespace Constants;

// ─── Helpers ────────────────────────────────────────────────────────────────

// Solve Kepler's equation M = E - e*sin(E) via Newton-Raphson
static double solveKepler(double M, double e) {
    double E = M;
    for (int i = 0; i < 50; ++i) {
        double dE = (M - E + e * std::sin(E)) / (1.0 - e * std::cos(E));
        E += dE;
        if (std::abs(dE) < 1e-12) break;
    }
    return E;
}

// Julian Date from Unix timestamp
static double unixToJulian(double unix_ts) {
    return unix_ts / 86400.0 + 2440587.5;
}

// GMST in radians from Unix timestamp
double computeGMST(double unix_timestamp) {
    double jd = unixToJulian(unix_timestamp);
    double T  = (jd - 2451545.0) / 36525.0;
    // GMST in seconds
    double theta = 67310.54841
                 + (876600.0 * 3600.0 + 8640184.812866) * T
                 + 0.093104 * T * T
                 - 6.2e-6   * T * T * T;
    theta = std::fmod(theta, 86400.0);
    if (theta < 0.0) theta += 86400.0;
    return theta * TWO_PI / 86400.0;
}

// ECI to Geodetic conversion
GeoPosition eciToGeodetic(const EciState& eci, double unix_timestamp) {
    double gmst = computeGMST(unix_timestamp);

    double x = eci.pos[0];
    double y = eci.pos[1];
    double z = eci.pos[2];

    // ECI -> ECEF: rotate by -GMST around Z
    double cosG  = std::cos(gmst), sinG = std::sin(gmst);
    double xecef =  x * cosG + y * sinG;
    double yecef = -x * sinG + y * cosG;
    double zecef =  z;

    // ECEF -> Geodetic (iterative Bowring)
    double a  = RE;           // km
    double b  = a * (1.0 - F);
    double e2 = 1.0 - (b * b) / (a * a);

    double lon = std::atan2(yecef, xecef);
    double p   = std::sqrt(xecef * xecef + yecef * yecef);
    double lat = std::atan2(zecef, p * (1.0 - e2));

    for (int i = 0; i < 10; ++i) {
        double sinLat = std::sin(lat);
        double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
        lat = std::atan2(zecef + e2 * N * sinLat, p);
    }

    double sinLat = std::sin(lat);
    double cosLat = std::cos(lat);
    double N   = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    double alt = (std::abs(cosLat) > 1e-10)
               ? (p / cosLat) - N
               : std::abs(zecef) / sinLat - N * (1.0 - e2);

    GeoPosition geo;
    geo.lat = lat * RAD2DEG;
    geo.lon = lon * RAD2DEG;
    geo.alt = alt;
    return geo;
}

// Stub for eciToGeo (uses GMST computed separately)
GeoPosition eciToGeo(const EciState& eci, double gmst_rad) {
    // For direct GMST usage — build a synthetic unix from GMST not needed here;
    // delegate to eciToGeodetic with approximate unix = 0 (GMST already baked in).
    // This function rotates ECI -> ECEF using the supplied GMST directly.
    double cosG  = std::cos(gmst_rad), sinG = std::sin(gmst_rad);
    double x = eci.pos[0], y = eci.pos[1], z = eci.pos[2];
    double xecef =  x * cosG + y * sinG;
    double yecef = -x * sinG + y * cosG;
    double zecef =  z;

    double a  = RE;
    double b  = a * (1.0 - F);
    double e2 = 1.0 - (b * b) / (a * a);

    double lon = std::atan2(yecef, xecef);
    double p   = std::sqrt(xecef * xecef + yecef * yecef);
    double lat = std::atan2(zecef, p * (1.0 - e2));

    for (int i = 0; i < 10; ++i) {
        double sinLat = std::sin(lat);
        double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
        lat = std::atan2(zecef + e2 * N * sinLat, p);
    }

    double sinLat = std::sin(lat);
    double cosLat = std::cos(lat);
    double N   = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    double alt = (std::abs(cosLat) > 1e-10)
               ? (p / cosLat) - N
               : std::abs(zecef) / sinLat - N * (1.0 - e2);

    GeoPosition geo;
    geo.lat = lat * RAD2DEG;
    geo.lon = lon * RAD2DEG;
    geo.alt = alt;
    return geo;
}

// ─── SGP4 ────────────────────────────────────────────────────────────────────

SGP4::SGP4(const TLEData& tle) {
    initialize(tle);
}

void SGP4::initialize(const TLEData& tle) {
    m_init.tle = tle;

    // Convert epoch JD to unix
    m_epochUnix = (tle.epoch - 2440587.5) * 86400.0;

    const double xke    = XKE;
    const double k2     = 0.5 * J2;
    const double k4     = -0.375 * J4;
    const double a3ovk2 = -1.2203986e-6 / k2; // -A30/k2

    double e0    = tle.ecco;
    double i0    = tle.inclo;
    double no    = tle.no_kozai;
    double bstar = tle.bstar;

    double cosi0  = std::cos(i0);
    double sini0  = std::sin(i0);
    double theta2 = cosi0 * cosi0;
    double x3thm1 = 3.0 * theta2 - 1.0;
    double eosq   = e0 * e0;
    double betao2 = 1.0 - eosq;
    double betao  = std::sqrt(betao2);

    // Recover original mean motion and semi-major axis
    double a1  = std::pow(xke / no, 2.0 / 3.0);
    double del1 = 1.5 * k2 * x3thm1 / (a1 * a1 * betao * betao2);
    double ao   = a1 * (1.0 - del1 * (1.0 / 3.0 + del1 * (1.0 + 134.0 / 81.0 * del1)));
    double del0 = 1.5 * k2 * x3thm1 / (ao * ao * betao * betao2);
    double no2  = no / (1.0 + del0);
    ao   = std::pow(xke / no2, 2.0 / 3.0);

    // Simple drag model parameters
    double po     = ao * betao2;
    double qoms2t = 1.88027916e-9; // (q0 - s)^4 in ER^4
    double s      = 1.01222928;    // s* in ER

    double pinvsq = 1.0 / (po * po);
    double tsi    = 1.0 / (ao - s);
    double eta    = ao * e0 * tsi;
    double etasq  = eta * eta;
    double eeta   = e0 * eta;
    double psisq  = std::abs(1.0 - etasq);
    double coef   = qoms2t * std::pow(tsi, 4.0);
    double coef1  = coef / std::pow(psisq, 3.5);
    double cc2    = coef1 * no2 * (ao * (1.0 + 1.5 * etasq + eeta * (4.0 + etasq))
                  + 0.75 * k2 * tsi / psisq * x3thm1 * (8.0 + 3.0 * etasq * (8.0 + etasq)));
    double cc1    = bstar * cc2;

    double cc3    = 0.0;
    if (e0 > 1.0e-4) {
        cc3 = -2.0 * coef * tsi * a3ovk2 * no2 * sini0 / e0;
    }

    double x1mth2 = 1.0 - theta2;
    double cc4    = 2.0 * no2 * coef1 * ao * betao2
                  * (eta * (2.0 + 0.5 * etasq) + e0 * (0.5 + 2.0 * etasq)
                  - k2 * tsi / (ao * psisq) * (-3.0 * x3thm1 * (1.0 - 2.0 * eeta + etasq * (1.5 - 0.5 * eeta))
                  + 0.75 * x1mth2 * (2.0 * etasq - eeta * (1.0 + etasq)) * std::cos(2.0 * tle.argpo)));
    double cc5    = 2.0 * coef1 * ao * betao2
                  * (1.0 + 2.75 * (etasq + eeta) + eeta * etasq);

    double theta4  = theta2 * theta2;
    double pinvsq2 = pinvsq * pinvsq;

    // Secular rates
    double mdot    = no2 + 0.5 * k2 * x3thm1 * pinvsq * no2 / betao
                   + 0.0625 * k2 * k2 * pinvsq2 * no2 * (13.0 - 78.0 * theta2 + 137.0 * theta4) / (betao * betao2);
    double argpdot = -0.5 * k2 * pinvsq * (1.0 - 5.0 * theta2) * no2 / betao2
                   + 0.0625 * k2 * k2 * pinvsq2 * (7.0 - 114.0 * theta2 + 395.0 * theta4) * no2 / (betao2 * betao2)
                   + 0.0625 * k4 * pinvsq2 * (3.0 - 36.0 * theta2 + 49.0 * theta4) * no2;
    double raandot = -k2 * cosi0 * no2 * pinvsq / betao2;

    double omgcof = bstar * cc3 * std::cos(tle.argpo);
    double sinmao = std::sin(tle.mo);

    double d2 = 0.0, d3 = 0.0, d4 = 0.0;
    double t3c = 0.0, t4c = 0.0, t5c = 0.0;
    bool   isimp = false;

    if (std::abs(cc1) > 1.0e-10) {
        d2 = 4.0 * ao * tsi * cc1 * cc1;
        double temp = d2 * tsi * cc1 / 3.0;
        d3  = (17.0 * ao + s) * temp;
        d4  = 0.5 * temp * ao * tsi * (221.0 * ao + 31.0 * s) * cc1;
        t3c = cc1 * cc1;
        t4c = d2 / 4.0;
        t5c = d3 / 5.0;
    } else {
        isimp = true;
    }

    double aycof  = -0.5 * a3ovk2 * sini0;
    double xlcof  = -0.25 * a3ovk2 * sini0 * (3.0 + 5.0 * cosi0) / (1.0 + cosi0);
    double con41  = 3.0 * theta2 - 1.0;
    double x7thm1 = 7.0 * theta2 - 1.0;

    m_init.mdot    = mdot;
    m_init.argpdot = argpdot;
    m_init.nodedot = raandot;
    m_init.cc1     = cc1;
    m_init.cc4     = cc4;
    m_init.cc5     = cc5;
    m_init.d2      = d2;
    m_init.d3      = d3;
    m_init.d4      = d4;
    m_init.eta     = eta;
    m_init.omgcof  = omgcof;
    m_init.sinmao  = sinmao;
    m_init.t2cof   = 1.5 * cc1;
    m_init.t3cof   = t3c;
    m_init.t4cof   = t4c;
    m_init.t5cof   = t5c;
    m_init.x1mth2  = x1mth2;
    m_init.x7thm1  = x7thm1;
    m_init.delmo   = std::pow(1.0 + eta * std::cos(tle.mo), 3.0);
    m_init.aycof   = aycof;
    m_init.xlcof   = xlcof;
    m_init.con41   = con41;
    m_init.isimp   = isimp;
    m_init.xlamo   = std::fmod(tle.mo + tle.nodeo + tle.argpo, TWO_PI);
}

EciState SGP4::propagate(double tsince) const {
    EciState result;
    result.valid = false;

    const auto& td = m_init.tle;
    const double xke = XKE;
    const double k2  = 0.5 * J2;

    double e0    = td.ecco;
    double i0    = td.inclo;
    double no    = td.no_kozai;
    double bstar = td.bstar;

    // Secular effects of atmospheric drag and gravitation
    double xmdf   = td.mo    + m_init.mdot    * tsince;
    double argpdf = td.argpo + m_init.argpdot * tsince;
    double nodedf = td.nodeo + m_init.nodedot * tsince;

    double t2    = tsince * tsince;
    double tempa = 1.0 - m_init.cc1 * tsince;
    double tempe = bstar * m_init.cc4 * tsince;
    double templ = m_init.t2cof * t2;

    double mm    = xmdf;
    double argpm = argpdf;

    if (!m_init.isimp) {
        double tcube = t2 * tsince;
        double tfour = tsince * tcube;
        tempa = tempa - m_init.d2 * t2 - m_init.d3 * tcube - m_init.d4 * tfour;
        tempe = tempe + bstar * m_init.cc5 * (std::sin(mm) - m_init.sinmao);
        templ = templ + m_init.t3cof * tcube + tfour * (m_init.t4cof + tsince * m_init.t5cof);
    }

    // Semi-major axis and mean motion
    double am = std::pow(xke / no, 2.0 / 3.0) * tempa * tempa;
    double nm = xke / std::pow(am, 1.5);
    double em = e0 - tempe;

    if (em < 1.0e-6) em = 1.0e-6;
    if (em >= 1.0)   em = 1.0 - 1.0e-6;

    mm = mm + no * templ;

    double nodem = std::fmod(nodedf, TWO_PI);
    argpm = std::fmod(argpdf, TWO_PI);
    double xlm = mm + argpm + nodem;
    mm = std::fmod(xlm - argpm - nodem, TWO_PI);
    if (mm < 0.0) mm += TWO_PI;

    // Solve Kepler's equation
    double E    = solveKepler(mm, em);
    double sinE = std::sin(E), cosE = std::cos(E);
    double emsq = em * em;

    // True anomaly
    double nu = std::atan2(std::sqrt(1.0 - emsq) * sinE, cosE - em);

    // Argument of latitude
    double u = nu + argpm;

    // Orbital radius in ER
    double r = am * (1.0 - em * cosE);

    // Build ECI position and velocity via perifocal->ECI rotation
    double cosi = std::cos(i0), sini = std::sin(i0);
    double cosuk = std::cos(u),  sinuk = std::sin(u);
    double cosnk = std::cos(nodem), sinnk = std::sin(nodem);

    // Unit vectors in ECI (perifocal frame rotated by RAAN and inclination)
    // P-hat (toward perigee), Q-hat (90° ahead in orbit plane)
    double Px = cosnk * cosuk - sinnk * sinuk * cosi;
    double Py = sinnk * cosuk + cosnk * sinuk * cosi;
    double Pz = sini  * sinuk;

    double Qx = -cosnk * sinuk - sinnk * cosuk * cosi;
    double Qy = -sinnk * sinuk + cosnk * cosuk * cosi;
    double Qz =  sini  * cosuk;

    // Position (ER)
    double xp = r * (Px * std::cos(nu) / std::cos(nu) ); // simplified: just use r * perifocal
    // Direct from u (argument of latitude = argument of perigee + true anomaly):
    xp = r * (cosnk * cosuk - sinnk * sinuk * cosi);
    double yp = r * (sinnk * cosuk + cosnk * sinuk * cosi);
    double zp = r * (sini  * sinuk);

    // Velocity (ER/min)
    double rdot  = xke * std::sqrt(am) * em * sinE / r;
    double rvdot = xke * std::sqrt(am * (1.0 - emsq)) / r;

    // r-hat and theta-hat
    double rhx = Px * std::cos(nu) + Qx * std::sin(nu);
    double rhy = Py * std::cos(nu) + Qy * std::sin(nu);
    double rhz = Pz * std::cos(nu) + Qz * std::sin(nu);

    // Correct: use position direction
    double rmag = r;
    rhx = xp / rmag;
    rhy = yp / rmag;
    rhz = zp / rmag;

    // theta-hat (perpendicular to r in orbit plane, in direction of motion)
    double thx = Qx * std::cos(nu) - Px * std::sin(nu);
    double thy = Qy * std::cos(nu) - Py * std::sin(nu);
    double thz = Qz * std::cos(nu) - Pz * std::sin(nu);

    // Use direct Q-hat for theta direction (at true anomaly nu)
    // theta-hat = d(r-hat)/d(nu): perpendicular to r-hat in orbit plane
    thx = -cosnk * sinuk - sinnk * cosuk * cosi;
    thy = -sinnk * sinuk + cosnk * cosuk * cosi;
    thz =  sini  * cosuk;

    double vx = rdot * rhx + rvdot * thx;
    double vy = rdot * rhy + rvdot * thy;
    double vz = rdot * rhz + rvdot * thz;

    // Convert ER to km, ER/min to km/s
    const double er2km     = XKMPER;
    const double ermin2kms = XKMPER / 60.0;

    result.pos[0] = xp * er2km;
    result.pos[1] = yp * er2km;
    result.pos[2] = zp * er2km;
    result.vel[0] = vx * ermin2kms;
    result.vel[1] = vy * ermin2kms;
    result.vel[2] = vz * ermin2kms;
    result.valid  = true;

    return result;
}

EciState SGP4::propagateUnix(double unix_timestamp) const {
    double tsince = (unix_timestamp - m_epochUnix) / 60.0;
    return propagate(tsince);
}

} // namespace OrbitEngine
