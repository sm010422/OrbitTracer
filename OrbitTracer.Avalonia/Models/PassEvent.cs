namespace OrbitTracer.Models;

public class PassEvent
{
    public string   SatelliteName  { get; set; } = string.Empty;
    public DateTime AosTime        { get; set; }
    public DateTime LosTime        { get; set; }
    public DateTime MaxElevTime    { get; set; }
    public double   MaxElevation   { get; set; }
    public double   AosAzimuth     { get; set; }
    public double   LosAzimuth     { get; set; }

    public TimeSpan Duration    => LosTime - AosTime;

    public string DurationStr => $"{(int)Duration.TotalMinutes}m {Duration.Seconds}s";
    public string MaxElevStr  => $"{MaxElevation:F1}\u00b0";
    public string AosAzStr    => $"{AosAzimuth:F0}\u00b0";
    public string LosAzStr    => $"{LosAzimuth:F0}\u00b0";
}
