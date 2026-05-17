using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using OrbitTracer.Models;
using OrbitTracer.Services;

namespace OrbitTracer.ViewModels;

public class MainViewModel : BaseViewModel, IDisposable
{
    private readonly OrbitEngineService _engine = new();
    private readonly DispatcherTimer    _timer  = new();

    // Satellite list
    public ObservableCollection<SatelliteViewModel> Satellites { get; } = new();

    // Ground stations
    public ObservableCollection<GroundStation> GroundStations { get; } = new()
    {
        GroundStation.Seoul,
        GroundStation.Daejeon,
        GroundStation.Sejong,
    };

    private GroundStation _selectedGS = GroundStation.Seoul;
    public GroundStation SelectedGroundStation
    {
        get => _selectedGS;
        set => SetField(ref _selectedGS, value);
    }

    private SatelliteViewModel? _selectedSat;
    public SatelliteViewModel? SelectedSatellite
    {
        get => _selectedSat;
        set
        {
            SetField(ref _selectedSat, value);
            OnPropertyChanged(nameof(HasSelection));
        }
    }

    public bool HasSelection => _selectedSat != null;

    private string _statusText = "Ready";
    public string StatusText
    {
        get => _statusText;
        set => SetField(ref _statusText, value);
    }

    private DateTime _currentTime = DateTime.Now;
    public DateTime CurrentTime
    {
        get => _currentTime;
        set => SetField(ref _currentTime, value);
    }

    private bool _isLoading;
    public bool IsLoading
    {
        get => _isLoading;
        set => SetField(ref _isLoading, value);
    }

    // Pass prediction results
    public ObservableCollection<PassEvent> Passes { get; } = new();

    // Commands
    public ICommand LoadDefaultTLECommand   { get; }
    public ICommand LoadTLEFileCommand      { get; }
    public ICommand PredictPassesCommand    { get; }
    public ICommand RemoveSatelliteCommand  { get; }
    public ICommand AddGroundStationCommand { get; }

    public MainViewModel()
    {
        LoadDefaultTLECommand   = new RelayCommand(async _ => await LoadDefaultTLEAsync());
        LoadTLEFileCommand      = new RelayCommand(_ => LoadTLEFile());
        PredictPassesCommand    = new RelayCommand(_ => PredictPasses(), _ => SelectedSatellite != null);
        RemoveSatelliteCommand  = new RelayCommand(_ => RemoveSelected(), _ => SelectedSatellite != null);
        AddGroundStationCommand = new RelayCommand(_ => AddGroundStation());

        _timer.Interval = TimeSpan.FromSeconds(1);
        _timer.Tick += OnTimerTick;
        _timer.Start();

        LoadDefaultTLE();
    }

    private void OnTimerTick(object? sender, EventArgs e)
    {
        CurrentTime = DateTime.Now;
        foreach (var sat in Satellites)
        {
            sat.UpdatePosition(_engine);
            if (SelectedSatellite != null)
                sat.UpdateLookAngle(_engine, SelectedGroundStation);
        }
    }

    private void LoadDefaultTLE()
    {
        // Example ISS TLE (placeholder — use "Fetch ISS" for a live TLE)
        const string name = "ISS (ZARYA)";
        const string l1   = "1 25544U 98067A   24001.50000000  .00006089  00000+0  11294-3 0  9993";
        const string l2   = "2 25544  51.6416 290.5000 0001234  80.0000 280.0000 15.49000000440000";

        try
        {
            int h = _engine.LoadSatellite(name, l1, l2);
            if (h >= 0)
                Satellites.Add(new SatelliteViewModel(h, name, l1, l2));
        }
        catch
        {
            // Ignore — TLE may be outdated or parsing may fail
        }
    }

    private async Task LoadDefaultTLEAsync()
    {
        IsLoading  = true;
        StatusText = "Fetching TLE data from CelesTrak...";
        try
        {
            var svc        = new TLEFetchService();
            string content = await svc.FetchISSAsync();
            int before     = Satellites.Count;
            ParseAndAddTLEs(content);
            StatusText = $"Loaded {Satellites.Count - before} satellites";
        }
        catch (Exception ex)
        {
            StatusText = $"Error: {ex.Message}";
            MessageBox.Show($"Failed to fetch TLE data:\n{ex.Message}", "Network Error",
                MessageBoxButton.OK, MessageBoxImage.Warning);
        }
        finally
        {
            IsLoading = false;
        }
    }

    private void LoadTLEFile()
    {
        var dlg = new Microsoft.Win32.OpenFileDialog
        {
            Title  = "Open TLE File",
            Filter = "TLE Files (*.tle;*.txt)|*.tle;*.txt|All Files (*.*)|*.*"
        };
        if (dlg.ShowDialog() != true) return;

        try
        {
            string content = File.ReadAllText(dlg.FileName);
            int before     = Satellites.Count;
            ParseAndAddTLEs(content);
            StatusText = $"Loaded {Satellites.Count - before} satellites from file";
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Error loading file:\n{ex.Message}", "Error",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void ParseAndAddTLEs(string content)
    {
        var lines = content.Split('\n', StringSplitOptions.None);
        int i = 0;
        while (i < lines.Length)
        {
            string a = lines[i].TrimEnd('\r');

            // Skip blank lines
            if (string.IsNullOrWhiteSpace(a)) { i++; continue; }

            bool aIsL1 = a.Length >= 2 && a[0] == '1' && a[1] == ' ';
            bool aIsL2 = a.Length >= 2 && a[0] == '2' && a[1] == ' ';

            if (!aIsL1 && !aIsL2 && i + 2 < lines.Length)
            {
                // a = name, try b = line1, c = line2
                string b = lines[i + 1].TrimEnd('\r');
                string c = lines[i + 2].TrimEnd('\r');
                bool bIsL1 = b.Length >= 2 && b[0] == '1' && b[1] == ' ';
                bool cIsL2 = c.Length >= 2 && c[0] == '2' && c[1] == ' ';

                if (bIsL1 && cIsL2)
                {
                    TryAddSatellite(a, b, c);
                    i += 3;
                    continue;
                }
            }
            else if (aIsL1 && i + 1 < lines.Length)
            {
                string b = lines[i + 1].TrimEnd('\r');
                bool bIsL2 = b.Length >= 2 && b[0] == '2' && b[1] == ' ';
                if (bIsL2)
                {
                    TryAddSatellite(string.Empty, a, b);
                    i += 2;
                    continue;
                }
            }

            i++;
        }
    }

    private void TryAddSatellite(string name, string line1, string line2)
    {
        try
        {
            int h = _engine.LoadSatellite(name.Trim(), line1, line2);
            if (h >= 0)
            {
                Application.Current.Dispatcher.Invoke(() =>
                    Satellites.Add(new SatelliteViewModel(h, name.Trim(), line1, line2)));
            }
        }
        catch
        {
            // Silently skip malformed TLE entries
        }
    }

    private void PredictPasses()
    {
        if (SelectedSatellite == null) return;

        Passes.Clear();
        var events = _engine.PredictPasses(
            SelectedSatellite.Handle,
            SelectedSatellite.Name,
            SelectedGroundStation,
            DateTime.UtcNow,
            durationHours: 48.0);

        foreach (var e in events)
            Passes.Add(e);

        StatusText = $"Found {Passes.Count} passes in next 48h for {SelectedSatellite.Name}";
    }

    private void RemoveSelected()
    {
        if (SelectedSatellite == null) return;
        _engine.FreeSatellite(SelectedSatellite.Handle);
        Satellites.Remove(SelectedSatellite);
        SelectedSatellite = null;
    }

    private void AddGroundStation()
    {
        var gs = new GroundStation
        {
            Name      = "Custom",
            Latitude  = 37.0,
            Longitude = 127.0,
            Altitude  = 0.1
        };
        GroundStations.Add(gs);
        SelectedGroundStation = gs;
    }

    public void Dispose()
    {
        _timer.Stop();
        _engine.Dispose();
    }
}

// ─── SatelliteViewModel ──────────────────────────────────────────────────────

public class SatelliteViewModel : BaseViewModel
{
    public int    Handle   { get; }
    public string Name     { get; }
    public string TleLine1 { get; }
    public string TleLine2 { get; }

    private double _latitude, _longitude, _altitude;
    private double _elevation = -90, _azimuth, _range;

    public double Latitude  { get => _latitude;  set => SetField(ref _latitude,  value); }
    public double Longitude { get => _longitude; set => SetField(ref _longitude, value); }
    public double Altitude  { get => _altitude;  set => SetField(ref _altitude,  value); }
    public double Elevation { get => _elevation; set => SetField(ref _elevation, value); }
    public double Azimuth   { get => _azimuth;   set => SetField(ref _azimuth,   value); }
    public double Range     { get => _range;     set => SetField(ref _range,     value); }

    public string PositionStr => $"{Latitude:F2}\u00b0, {Longitude:F2}\u00b0  Alt: {Altitude:F0} km";
    public string LookStr     => $"El: {Elevation:F1}\u00b0  Az: {Azimuth:F1}\u00b0  {Range:F0} km";
    public bool   IsVisible   => Elevation > 5.0;

    public SatelliteViewModel(int handle, string name, string line1, string line2)
    {
        Handle   = handle;
        Name     = name;
        TleLine1 = line1;
        TleLine2 = line2;
    }

    public void UpdatePosition(OrbitEngineService engine)
    {
        var pos = engine.GetPosition(Handle);
        if (pos.HasValue)
        {
            Latitude  = pos.Value.lat;
            Longitude = pos.Value.lon;
            Altitude  = pos.Value.alt;
            OnPropertyChanged(nameof(PositionStr));
            OnPropertyChanged(nameof(IsVisible));
        }
    }

    public void UpdateLookAngle(OrbitEngineService engine, GroundStation gs)
    {
        var aa = engine.GetLookAngle(Handle, gs);
        if (aa.HasValue)
        {
            Elevation = aa.Value.elev;
            Azimuth   = aa.Value.az;
            Range     = aa.Value.range;
            OnPropertyChanged(nameof(LookStr));
            OnPropertyChanged(nameof(IsVisible));
        }
    }
}

// ─── RelayCommand ────────────────────────────────────────────────────────────

public class RelayCommand : ICommand
{
    private readonly Action<object?> _execute;
    private readonly Func<object?, bool>? _canExecute;

    public RelayCommand(Action<object?> execute, Func<object?, bool>? canExecute = null)
    {
        _execute    = execute;
        _canExecute = canExecute;
    }

    public event EventHandler? CanExecuteChanged
    {
        add    => CommandManager.RequerySuggested += value;
        remove => CommandManager.RequerySuggested -= value;
    }

    public bool CanExecute(object? parameter) => _canExecute?.Invoke(parameter) ?? true;
    public void Execute(object? parameter)    => _execute(parameter);
}
