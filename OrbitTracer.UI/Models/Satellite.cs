namespace OrbitTracer.Models;

public class Satellite
{
    public int    Handle    { get; set; }
    public string Name      { get; set; } = string.Empty;
    public string TleLine1  { get; set; } = string.Empty;
    public string TleLine2  { get; set; } = string.Empty;

    // Current position (updated in real-time)
    public double Latitude  { get; set; }
    public double Longitude { get; set; }
    public double Altitude  { get; set; }
}
