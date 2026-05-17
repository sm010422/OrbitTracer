#pragma once
#include "SGP4.h"
#include <vector>
#include <string>

namespace OrbitEngine {

class TLEParser {
public:
    // Parse a single TLE (name line + 2 data lines)
    static TLEData parseTLE(const std::string& name,
                            const std::string& line1,
                            const std::string& line2);

    // Parse multiple TLEs from a file or string buffer
    static std::vector<TLEData> parseFile(const std::string& filepath);
    static std::vector<TLEData> parseString(const std::string& content);

private:
    static double parseEpoch(const std::string& epochStr);
    static double parseDecimalAssumedPoint(const std::string& s);
    static double julianToUnix(double jd);
};

} // namespace OrbitEngine
