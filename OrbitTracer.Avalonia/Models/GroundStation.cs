namespace OrbitTracer.Models;

public class GroundStation
{
    public string Name      { get; set; } = string.Empty;
    public double Latitude  { get; set; }
    public double Longitude { get; set; }
    public double Altitude  { get; set; } // km

    // Presets
    public static GroundStation Seoul => new()
        { Name = "Seoul", Latitude = 37.5665, Longitude = 126.9780, Altitude = 0.038 };

    public static GroundStation Daejeon => new()
        { Name = "Daejeon (KAIST)", Latitude = 36.3504, Longitude = 127.3845, Altitude = 0.077 };

    public static GroundStation Sejong => new()
        { Name = "Sejong", Latitude = 36.4801, Longitude = 127.2890, Altitude = 0.032 };
}
