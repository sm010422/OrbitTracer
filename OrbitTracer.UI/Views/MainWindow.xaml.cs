using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;
using System.Windows.Threading;
using OrbitTracer.ViewModels;

namespace OrbitTracer.Views;

public partial class MainWindow : Window
{
    private MainViewModel _vm = null!;
    private DispatcherTimer _mapTimer = null!;

    public MainWindow()
    {
        InitializeComponent();
        Loaded  += OnLoaded;
        Closing += OnClosing;
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        _vm = (MainViewModel)DataContext;

        DrawWorldGrid();

        // Redraw map every 2 seconds to update satellite positions
        _mapTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(2) };
        _mapTimer.Tick += (_, _) => UpdateSatelliteMarkers();
        _mapTimer.Start();
    }

    private void MapCanvas_SizeChanged(object sender, SizeChangedEventArgs e)
    {
        DrawWorldGrid();
        UpdateSatelliteMarkers();
    }

    private void DrawWorldGrid()
    {
        if (MapCanvas == null) return;

        double w = MapCanvas.ActualWidth;
        double h = MapCanvas.ActualHeight;
        if (w < 1 || h < 1) return;

        MapCanvas.Children.Clear();

        var gridBrush = new SolidColorBrush(Color.FromArgb(30, 255, 255, 255));
        var equatorBrush = new SolidColorBrush(Color.FromArgb(60, 0, 200, 100));

        // Longitude lines every 30 degrees
        for (int lon = -180; lon <= 180; lon += 30)
        {
            double x = (lon + 180.0) / 360.0 * w;
            MapCanvas.Children.Add(new Line
            {
                X1 = x, Y1 = 0, X2 = x, Y2 = h,
                Stroke          = gridBrush,
                StrokeThickness = 1
            });
        }

        // Latitude lines every 30 degrees
        for (int lat = -90; lat <= 90; lat += 30)
        {
            double y = (90.0 - lat) / 180.0 * h;
            MapCanvas.Children.Add(new Line
            {
                X1 = 0, Y1 = y, X2 = w, Y2 = y,
                Stroke          = lat == 0 ? equatorBrush : gridBrush,
                StrokeThickness = lat == 0 ? 1.5 : 1
            });
        }

        // Labels for key longitudes
        foreach (var (lonVal, label) in new[] { (-180, "-180"), (-90, "-90"), (0, "0"), (90, "+90"), (180, "180") })
        {
            double x = (lonVal + 180.0) / 360.0 * w;
            var tb = new TextBlock
            {
                Text       = label,
                FontSize   = 9,
                Foreground = new SolidColorBrush(Color.FromArgb(80, 200, 200, 200)),
            };
            Canvas.SetLeft(tb, x + 2);
            Canvas.SetTop(tb, h - 14);
            MapCanvas.Children.Add(tb);
        }
    }

    private void UpdateSatelliteMarkers()
    {
        if (_vm == null || MapCanvas == null) return;

        // Remove only satellite markers (Ellipses and marker TextBlocks added after the grid)
        // Simplest approach: redraw everything
        DrawWorldGrid();

        double w = MapCanvas.ActualWidth;
        double h = MapCanvas.ActualHeight;
        if (w < 1 || h < 1) return;

        foreach (var sat in _vm.Satellites)
        {
            double x = (sat.Longitude + 180.0) / 360.0 * w;
            double y = (90.0 - sat.Latitude)   / 180.0 * h;

            bool selected = sat == _vm.SelectedSatellite;
            double size   = selected ? 12.0 : 8.0;

            Color color = sat.IsVisible
                ? Color.FromRgb(0, 255, 136)
                : Color.FromRgb(100, 100, 200);

            var ellipse = new Ellipse
            {
                Width           = size,
                Height          = size,
                Fill            = new SolidColorBrush(color),
                Stroke          = selected ? new SolidColorBrush(Colors.White) : null,
                StrokeThickness = selected ? 2 : 0,
                ToolTip         = $"{sat.Name}\n{sat.PositionStr}",
            };
            Canvas.SetLeft(ellipse, x - size / 2);
            Canvas.SetTop(ellipse,  y - size / 2);
            MapCanvas.Children.Add(ellipse);

            // Label for selected satellite
            if (selected)
            {
                var label = new TextBlock
                {
                    Text       = sat.Name,
                    FontSize   = 10,
                    Foreground = new SolidColorBrush(Colors.White),
                };
                Canvas.SetLeft(label, x + 8);
                Canvas.SetTop(label,  y - 6);
                MapCanvas.Children.Add(label);
            }
        }

        // Draw ground station marker
        if (_vm.SelectedGroundStation != null)
        {
            double gx = (_vm.SelectedGroundStation.Longitude + 180.0) / 360.0 * w;
            double gy = (90.0 - _vm.SelectedGroundStation.Latitude)   / 180.0 * h;

            var gsMarker = new Polygon
            {
                Points = new PointCollection(new[]
                {
                    new Point(gx, gy - 8),
                    new Point(gx - 5, gy + 4),
                    new Point(gx + 5, gy + 4),
                }),
                Fill            = new SolidColorBrush(Color.FromRgb(233, 69, 96)),
                Stroke          = new SolidColorBrush(Colors.White),
                StrokeThickness = 1,
                ToolTip         = $"GS: {_vm.SelectedGroundStation.Name}",
            };
            MapCanvas.Children.Add(gsMarker);
        }
    }

    private void OnClosing(object? sender, System.ComponentModel.CancelEventArgs e)
    {
        _mapTimer?.Stop();
        _vm?.Dispose();
    }
}
