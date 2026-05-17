using System.Runtime.InteropServices;
using System.Text;
using OrbitTracer.Models;

namespace OrbitTracer.Services;

[StructLayout(LayoutKind.Sequential)]
public struct GeoPositionC
{
    public double Lat, Lon, Alt;
}

[StructLayout(LayoutKind.Sequential)]
public struct AltAzC
{
    public double Elevation, Azimuth, RangeKm;
}

[StructLayout(LayoutKind.Sequential)]
public struct PassEventC
{
    public double AosUnix, LosUnix, MaxElev, MaxElevUnix, AosAz, LosAz;
}

public static class OrbitEngineNative
{
    private const string DllName = "OrbitEngine";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int orbit_init(string name, string line1, string line2);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void orbit_free(int handle);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int orbit_get_position(int handle, double unix_ts, out GeoPositionC pos);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int orbit_get_look_angle(int handle, double unix_ts,
        double gs_lat, double gs_lon, double gs_alt, out AltAzC altaz);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int orbit_predict_passes(int handle,
        double gs_lat, double gs_lon, double gs_alt,
        double start_unix, double duration_sec, double min_elev_deg,
        [Out] PassEventC[] events, int max_passes);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int orbit_load_tle_file(string filepath, [Out] int[] handles, int max_handles);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void orbit_get_name(int handle, StringBuilder buf, int buf_len);
}

public class OrbitEngineService : IDisposable
{
    private readonly List<int> _handles = new();
    private bool _disposed;

    private static double ToUnix(DateTime dt) =>
        (dt.ToUniversalTime() - DateTime.UnixEpoch).TotalSeconds;

    private static DateTime FromUnix(double unix) =>
        DateTime.UnixEpoch.AddSeconds(unix).ToLocalTime();

    public int LoadSatellite(string name, string line1, string line2)
    {
        int h = OrbitEngineNative.orbit_init(name, line1, line2);
        if (h >= 0) _handles.Add(h);
        return h;
    }

    public void FreeSatellite(int handle)
    {
        OrbitEngineNative.orbit_free(handle);
        _handles.Remove(handle);
    }

    public (double lat, double lon, double alt)? GetPosition(int handle, DateTime? at = null)
    {
        double unix = ToUnix(at ?? DateTime.UtcNow);
        int r = OrbitEngineNative.orbit_get_position(handle, unix, out GeoPositionC pos);
        return r == 0 ? (pos.Lat, pos.Lon, pos.Alt) : null;
    }

    public (double elev, double az, double range)? GetLookAngle(
        int handle, GroundStation gs, DateTime? at = null)
    {
        double unix = ToUnix(at ?? DateTime.UtcNow);
        int r = OrbitEngineNative.orbit_get_look_angle(
            handle, unix, gs.Latitude, gs.Longitude, gs.Altitude, out AltAzC aa);
        return r == 0 ? (aa.Elevation, aa.Azimuth, aa.RangeKm) : null;
    }

    public List<PassEvent> PredictPasses(
        int handle, string satName,
        GroundStation gs,
        DateTime start, double durationHours = 24.0,
        double minElevDeg = 5.0)
    {
        const int MaxPasses = 100;
        var events = new PassEventC[MaxPasses];
        int count = OrbitEngineNative.orbit_predict_passes(
            handle, gs.Latitude, gs.Longitude, gs.Altitude,
            ToUnix(start.ToUniversalTime()), durationHours * 3600.0,
            minElevDeg, events, MaxPasses);

        var result = new List<PassEvent>();
        if (count <= 0) return result;

        for (int i = 0; i < count; ++i)
        {
            result.Add(new PassEvent
            {
                SatelliteName = satName,
                AosTime       = FromUnix(events[i].AosUnix),
                LosTime       = FromUnix(events[i].LosUnix),
                MaxElevTime   = FromUnix(events[i].MaxElevUnix),
                MaxElevation  = events[i].MaxElev,
                AosAzimuth    = events[i].AosAz,
                LosAzimuth    = events[i].LosAz,
            });
        }
        return result;
    }

    public void Dispose()
    {
        if (_disposed) return;
        foreach (var h in _handles)
            OrbitEngineNative.orbit_free(h);
        _handles.Clear();
        _disposed = true;
    }
}
