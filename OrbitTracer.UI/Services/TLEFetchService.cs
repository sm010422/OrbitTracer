using System.Net.Http;

namespace OrbitTracer.Services;

public class TLEFetchService
{
    private static readonly HttpClient _http = new();

    // Fetch from CelesTrak by group name (e.g. "stations", "weather", "visual")
    public async Task<string> FetchGroupAsync(string groupName, CancellationToken ct = default)
    {
        string url = $"https://celestrak.org/pub/TLE/{groupName.ToLower()}.txt";
        var response = await _http.GetAsync(url, ct);
        response.EnsureSuccessStatusCode();
        return await response.Content.ReadAsStringAsync(ct);
    }

    // Fetch space stations (includes ISS)
    public async Task<string> FetchISSAsync(CancellationToken ct = default)
    {
        const string url = "https://celestrak.org/pub/TLE/stations.txt";
        var response = await _http.GetAsync(url, ct);
        response.EnsureSuccessStatusCode();
        return await response.Content.ReadAsStringAsync(ct);
    }

    // Fetch weather satellites
    public async Task<string> FetchWeatherAsync(CancellationToken ct = default)
    {
        const string url = "https://celestrak.org/pub/TLE/weather.txt";
        var response = await _http.GetAsync(url, ct);
        response.EnsureSuccessStatusCode();
        return await response.Content.ReadAsStringAsync(ct);
    }

    // Fetch active satellites (brightest / most tracked)
    public async Task<string> FetchActiveAsync(CancellationToken ct = default)
    {
        const string url = "https://celestrak.org/pub/TLE/active.txt";
        var response = await _http.GetAsync(url, ct);
        response.EnsureSuccessStatusCode();
        return await response.Content.ReadAsStringAsync(ct);
    }
}
