using System;
using System.IO;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Shapes;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using OrbitTracer.ViewModels;

namespace OrbitTracer.Views;

public partial class MainWindow : Window
{
    private MainViewModel ViewModel => (MainViewModel)DataContext!;

    // Map redraw timer — 2 s interval, same cadence as the WPF version
    private readonly DispatcherTimer _mapTimer = new() { Interval = TimeSpan.FromSeconds(2) };

    public MainWindow()
    {
        InitializeComponent();
        DataContext = new MainViewModel();
    }

    protected override void OnOpened(EventArgs e)
    {
        base.OnOpened(e);

        // Draw the map immediately then on each tick
        _mapTimer.Tick += (_, _) => DrawMap();
        _mapTimer.Start();
        DrawMap();
    }

    protected override void OnClosed(EventArgs e)
    {
        _mapTimer.Stop();
        ViewModel.Dispose();
        base.OnClosed(e);
    }

    // ── File picker (replaces WPF's Microsoft.Win32.OpenFileDialog) ──────────
    private async void OnOpenFileClick(object? sender, RoutedEventArgs e)
    {
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title       = "TLE 파일 열기",
            AllowMultiple = false,
            FileTypeFilter = new[]
            {
                new FilePickerFileType("TLE Files") { Patterns = new[] { "*.tle", "*.txt" } },
                new FilePickerFileType("All Files")  { Patterns = new[] { "*" } }
            }
        });

        if (files.Count == 0) return;

        await using var stream = await files[0].OpenReadAsync();
        using var reader = new StreamReader(stream);
        string content = await reader.ReadToEndAsync();
        ViewModel.LoadTLEContent(content);
    }

    // ── World-map canvas renderer ─────────────────────────────────────────────
    // Uses Avalonia.Controls.Shapes — same Canvas.SetLeft/Top API as WPF
    private void DrawMap()
    {
        if (MapCanvas is null) return;

        double w = MapCanvas.Bounds.Width;
        double h = MapCanvas.Bounds.Height;
        if (w < 1 || h < 1) return;

        MapCanvas.Children.Clear();

        var gridBrush    = new SolidColorBrush(Color.FromArgb(30,  255, 255, 255));
        var equatorBrush = new SolidColorBrush(Color.FromArgb(60,    0, 200, 100));

        // Longitude lines every 30°
        for (int lon = -180; lon <= 180; lon += 30)
        {
            double x = (lon + 180.0) / 360.0 * w;
            MapCanvas.Children.Add(new Line
            {
                StartPoint      = new Point(x, 0),
                EndPoint        = new Point(x, h),
                Stroke          = gridBrush,
                StrokeThickness = 1
            });
        }

        // Latitude lines every 30°
        for (int lat = -90; lat <= 90; lat += 30)
        {
            double y = (90.0 - lat) / 180.0 * h;
            MapCanvas.Children.Add(new Line
            {
                StartPoint      = new Point(0, y),
                EndPoint        = new Point(w, y),
                Stroke          = lat == 0 ? equatorBrush : gridBrush,
                StrokeThickness = lat == 0 ? 1.5 : 1
            });
        }

        // Longitude labels
        foreach (var (lonVal, label) in new[]
        {
            (-180, "-180"), (-90, "-90"), (0, "0"), (90, "+90"), (180, "180")
        })
        {
            double x = (lonVal + 180.0) / 360.0 * w;
            var tb = new TextBlock
            {
                Text       = label,
                FontSize   = 9,
                Foreground = new SolidColorBrush(Color.FromArgb(80, 200, 200, 200))
            };
            Canvas.SetLeft(tb, x + 2);
            Canvas.SetTop (tb, h - 14);
            MapCanvas.Children.Add(tb);
        }

        // Satellite markers
        foreach (var sat in ViewModel.Satellites)
        {
            double x = (sat.Longitude + 180.0) / 360.0 * w;
            double y = (90.0 - sat.Latitude)   / 180.0 * h;

            bool   selected = sat == ViewModel.SelectedSatellite;
            double size     = selected ? 12.0 : 8.0;

            Color color = sat.IsVisible
                ? Color.FromRgb(0, 255, 136)
                : Color.FromRgb(100, 100, 200);

            var ellipse = new Ellipse
            {
                Width           = size,
                Height          = size,
                Fill            = new SolidColorBrush(color),
                Stroke          = selected ? Brushes.White : null,
                StrokeThickness = selected ? 2 : 0,
                [ToolTip.TipProperty] = $"{sat.Name}\n{sat.PositionStr}"
            };

            Canvas.SetLeft(ellipse, x - size / 2);
            Canvas.SetTop (ellipse, y - size / 2);
            MapCanvas.Children.Add(ellipse);

            // Name label for the selected satellite
            if (selected)
            {
                var label = new TextBlock
                {
                    Text       = sat.Name,
                    FontSize   = 10,
                    Foreground = Brushes.White
                };
                Canvas.SetLeft(label, x + 8);
                Canvas.SetTop (label, y - 6);
                MapCanvas.Children.Add(label);
            }
        }

        // Ground station triangle marker
        var gs = ViewModel.SelectedGroundStation;
        if (gs != null)
        {
            double gx = (gs.Longitude + 180.0) / 360.0 * w;
            double gy = (90.0 - gs.Latitude)   / 180.0 * h;

            var triangle = new Polygon
            {
                Points = new Points(new[]
                {
                    new Point(gx,     gy - 8),
                    new Point(gx - 5, gy + 4),
                    new Point(gx + 5, gy + 4)
                }),
                Fill            = new SolidColorBrush(Color.FromRgb(233, 69, 96)),
                Stroke          = Brushes.White,
                StrokeThickness = 1,
                [ToolTip.TipProperty] = $"GS: {gs.Name}"
            };
            MapCanvas.Children.Add(triangle);
        }
    }
}
