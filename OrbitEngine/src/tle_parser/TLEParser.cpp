#include "TLEParser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace OrbitEngine {

using namespace Constants;

// Parse assumed-decimal-point exponential notation used in TLE fields.
// e.g. " 12345-3" => 0.12345e-3,  "+12345-3" => 0.12345e-3
static double parseImpField(const std::string& raw) {
    // Trim spaces
    size_t start = raw.find_first_not_of(' ');
    if (start == std::string::npos) return 0.0;
    std::string s = raw.substr(start);
    if (s.empty()) return 0.0;

    int sign = 1;
    size_t pos = 0;
    if (s[0] == '-') { sign = -1; pos = 1; }
    else if (s[0] == '+') { pos = 1; }

    // Find the exponent sign (second sign character after the mantissa)
    size_t epos = std::string::npos;
    for (size_t i = pos; i < s.size(); ++i) {
        if (s[i] == '-' || s[i] == '+') { epos = i; break; }
    }

    if (epos == std::string::npos) {
        // No exponent — pure mantissa with assumed decimal
        std::string mantissa = s.substr(pos);
        if (mantissa.empty()) return 0.0;
        return sign * std::stod("0." + mantissa);
    }

    std::string mantissa = s.substr(pos, epos - pos);
    std::string exponent = s.substr(epos);
    if (mantissa.empty()) return 0.0;
    return sign * std::stod("0." + mantissa + "e" + exponent);
}

// Parse TLE epoch string "YYDDD.DDDDDDDD" to Julian Date
static double parseEpochStr(const std::string& epochStr) {
    // Trim
    std::string e = epochStr;
    size_t start = e.find_first_not_of(' ');
    if (start != std::string::npos) e = e.substr(start);

    int year2 = std::stoi(e.substr(0, 2));
    int year  = (year2 >= 57) ? 1900 + year2 : 2000 + year2;
    double doy = std::stod(e.substr(2));

    // Julian Date of Jan 1.0 of year (Jan 0.0 = Dec 31.0 of previous year)
    // Using the standard algorithm:
    int Y  = year;
    double A = std::floor((Y - 1.0) / 100.0);
    double B = 2.0 - A + std::floor(A / 4.0);
    // JD of Jan 0.5 (noon, Dec 31) of year Y-1:
    double jd_jan1_noon = std::floor(365.25 * (Y - 1)) + std::floor(30.6001 * 14)
                        + 1720994.5 + B;
    // jd_jan1_noon is actually JD of Jan 0.0 (midnight start of year)
    // Standard: JD of Jan 1.0 of year Y
    double jd_jan1 = std::floor(365.25 * (Y + 4716)) + std::floor(30.6001 * 14)
                   - 1524.5 + B - 4716 * 365.25;

    // Simpler and more reliable:
    // JD of Jan 1.0 (midnight) of year Y
    // Use: compute JD for Y Jan 1 00:00:00 UTC
    int m = 1, d = 1;
    int yy = Y;
    if (m <= 2) { yy -= 1; m += 12; }
    int aa = (int)std::floor(yy / 100.0);
    int bb = 2 - aa + (int)std::floor(aa / 4.0);
    double jd0 = std::floor(365.25 * (yy + 4716))
               + std::floor(30.6001 * (m + 1))
               + d + bb - 1524.5;

    return jd0 + (doy - 1.0); // doy is 1-based, so subtract 1 to get days offset
}

TLEData TLEParser::parseTLE(const std::string& name,
                             const std::string& line1,
                             const std::string& line2) {
    TLEData tle;
    tle.name  = name;
    // Trim trailing whitespace from name
    size_t end = tle.name.find_last_not_of(" \t\r\n");
    if (end != std::string::npos) tle.name = tle.name.substr(0, end + 1);

    tle.line1 = line1;
    tle.line2 = line2;

    if (line1.size() < 69 || line2.size() < 69)
        throw std::invalid_argument("TLE line too short");

    // ── Line 1 ──────────────────────────────────────────────────────────────
    // Cols  1    : line number '1'
    // Cols  3-7  : satellite number
    // Cols  9    : classification
    // Cols 10-17 : COSPAR ID (launch year, launch number, piece)
    // Cols 19-32 : Epoch (YYDDD.DDDDDDDD)
    // Cols 34-43 : First time derivative of mean motion / 2
    // Cols 45-52 : Second time derivative of mean motion / 6 (decimal point assumed)
    // Cols 54-61 : BSTAR drag term (decimal point assumed)
    // Cols 63    : Ephemeris type
    // Cols 65-68 : Element set number
    // Col  69    : Checksum
    tle.satnum = std::stoi(line1.substr(2, 5));
    tle.epoch  = parseEpochStr(line1.substr(18, 14));

    // ndot: first time derivative of mean motion (revs/day^2 * 2, stored without factor)
    // stored as decimal with sign in cols 34-43
    std::string ndotStr = line1.substr(33, 10);
    tle.ndot = std::stod(ndotStr) / (MIN_PER_DAY * MIN_PER_DAY); // rev/day^2 -> rad/min^2: divide by 2 already in TLE

    // nddot: second time derivative / 6 (assumed decimal, expontent format)
    tle.nddot = parseImpField(line1.substr(44, 8)) / (MIN_PER_DAY * MIN_PER_DAY * MIN_PER_DAY);

    // BSTAR
    tle.bstar = parseImpField(line1.substr(53, 8));

    // ── Line 2 ──────────────────────────────────────────────────────────────
    tle.inclo    = std::stod(line2.substr(8,  8)) * DEG2RAD;
    tle.nodeo    = std::stod(line2.substr(17, 8)) * DEG2RAD;
    tle.ecco     = std::stod("0." + line2.substr(26, 7));
    tle.argpo    = std::stod(line2.substr(34, 8)) * DEG2RAD;
    tle.mo       = std::stod(line2.substr(43, 8)) * DEG2RAD;
    // Mean motion in rev/day -> rad/min
    tle.no_kozai = std::stod(line2.substr(52, 11)) * TWO_PI / MIN_PER_DAY;

    return tle;
}

std::vector<TLEData> TLEParser::parseString(const std::string& content) {
    std::vector<TLEData> result;
    std::istringstream ss(content);
    std::vector<std::string> lines;
    std::string line;

    while (std::getline(ss, line)) {
        // Remove CR
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    // Remove blank lines for indexing purposes but keep original for name
    // Strategy: scan for '1 ' and '2 ' prefixes
    size_t i = 0;
    while (i < lines.size()) {
        // Skip blank lines
        if (lines[i].empty() || lines[i].find_first_not_of(" \t") == std::string::npos) {
            ++i; continue;
        }

        // Check if lines[i] is a name line (doesn't start with '1 ' or '2 ')
        bool isLine1 = (lines[i].size() >= 2 && lines[i][0] == '1' && lines[i][1] == ' ');
        bool isLine2 = (lines[i].size() >= 2 && lines[i][0] == '2' && lines[i][1] == ' ');

        if (!isLine1 && !isLine2) {
            // This might be a name line
            // Look ahead for TLE lines
            size_t j = i + 1;
            while (j < lines.size() && (lines[j].empty() || lines[j].find_first_not_of(" \t") == std::string::npos)) ++j;
            size_t k = j + 1;
            while (k < lines.size() && (lines[k].empty() || lines[k].find_first_not_of(" \t") == std::string::npos)) ++k;

            if (j < lines.size() && k < lines.size()) {
                bool jIsL1 = (lines[j].size() >= 2 && lines[j][0] == '1' && lines[j][1] == ' ');
                bool kIsL2 = (lines[k].size() >= 2 && lines[k][0] == '2' && lines[k][1] == ' ');
                if (jIsL1 && kIsL2) {
                    try {
                        result.push_back(parseTLE(lines[i], lines[j], lines[k]));
                    } catch (...) {}
                    i = k + 1;
                    continue;
                }
            }
            // Not a valid group — skip this line
            ++i;
        } else if (isLine1) {
            // Look for line2 right after (possibly with blanks)
            size_t j = i + 1;
            while (j < lines.size() && (lines[j].empty() || lines[j].find_first_not_of(" \t") == std::string::npos)) ++j;
            if (j < lines.size()) {
                bool jIsL2 = (lines[j].size() >= 2 && lines[j][0] == '2' && lines[j][1] == ' ');
                if (jIsL2) {
                    try {
                        result.push_back(parseTLE("", lines[i], lines[j]));
                    } catch (...) {}
                    i = j + 1;
                    continue;
                }
            }
            ++i;
        } else {
            ++i;
        }
    }

    return result;
}

std::vector<TLEData> TLEParser::parseFile(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open())
        throw std::runtime_error("Cannot open file: " + filepath);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return parseString(content);
}

double TLEParser::parseEpoch(const std::string& epochStr) {
    return parseEpochStr(epochStr);
}

double TLEParser::parseDecimalAssumedPoint(const std::string& s) {
    return parseImpField(s);
}

double TLEParser::julianToUnix(double jd) {
    return (jd - 2440587.5) * 86400.0;
}

} // namespace OrbitEngine
