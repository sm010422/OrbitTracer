using System.Collections.ObjectModel;
using OrbitTracer.Models;

namespace OrbitTracer.ViewModels;

public class PassPredictionViewModel : BaseViewModel
{
    public ObservableCollection<PassEvent> Passes { get; } = new();

    private double _minElevation = 5.0;
    public double MinElevation
    {
        get => _minElevation;
        set => SetField(ref _minElevation, value);
    }

    private double _durationHours = 24.0;
    public double DurationHours
    {
        get => _durationHours;
        set => SetField(ref _durationHours, value);
    }
}
