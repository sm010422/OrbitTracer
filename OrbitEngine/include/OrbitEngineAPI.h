#pragma once

#ifdef _WIN32
  #define ORBIT_API __declspec(dllexport)
#else
  #define ORBIT_API __attribute__((visibility("default")))
#endif

extern "C" {

struct PassEventC {
    double aos_unix;
    double los_unix;
    double max_elev;
    double max_elev_unix;
    double aos_az;
    double los_az;
};

struct GeoPositionC {
    double lat;
    double lon;
    double alt;
};

struct AltAzC {
    double elevation;
    double azimuth;
    double range_km;
};

// Initialize a satellite from TLE. Returns handle (>= 0) or -1 on error.
ORBIT_API int  orbit_init(const char* name, const char* line1, const char* line2);

// Release satellite handle
ORBIT_API void orbit_free(int handle);

// Get satellite geodetic position at unix timestamp
ORBIT_API int  orbit_get_position(int handle, double unix_ts, GeoPositionC* out);

// Get look angles from ground station to satellite
ORBIT_API int  orbit_get_look_angle(int handle,
                                     double unix_ts,
                                     double gs_lat, double gs_lon, double gs_alt,
                                     AltAzC* out);

// Predict passes. Returns count of passes found (up to max_passes).
ORBIT_API int  orbit_predict_passes(int handle,
                                     double gs_lat, double gs_lon, double gs_alt,
                                     double start_unix, double duration_sec,
                                     double min_elev_deg,
                                     PassEventC* events, int max_passes);

// Parse TLE file, returns count of satellites loaded into internal list
ORBIT_API int  orbit_load_tle_file(const char* filepath, int* handles, int max_handles);

// Get TLE name for a handle
ORBIT_API void orbit_get_name(int handle, char* buf, int buf_len);

} // extern "C"
