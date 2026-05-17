# MVVM 패턴 학습 노트

## MVVM이란?

Model-View-ViewModel. WPF/Avalonia의 표준 아키텍처 패턴입니다.

```
┌────────────┐    데이터 바인딩    ┌────────────────┐
│    View    │◄──────────────────►│   ViewModel    │
│  (.axaml)  │    Command 패턴     │  (C# 클래스)   │
└────────────┘                    └───────┬────────┘
                                          │ 사용
                                   ┌──────▼──────┐
                                   │    Model    │
                                   │ (순수 데이터) │
                                   └─────────────┘
```

**핵심 원칙**: View는 ViewModel을 알지만, **ViewModel은 View를 모릅니다**.
→ ViewModel을 UI 없이 단독으로 테스트 가능

---

## 이 프로젝트의 MVVM 구조

### Model — 순수 데이터

```csharp
// Models/PassEvent.cs
public class PassEvent
{
    public DateTime AosTime      { get; set; }
    public DateTime LosTime      { get; set; }
    public double   MaxElevation { get; set; }
    public double   AosAzimuth   { get; set; }

    // 표시용 계산 프로퍼티 (UI 무관한 순수 계산)
    public TimeSpan Duration    => LosTime - AosTime;
    public string   DurationStr => $"{(int)Duration.TotalMinutes}m {Duration.Seconds}s";
    public string   MaxElevStr  => $"{MaxElevation:F1}°";
}
```

### ViewModel — UI 상태 + 로직

```csharp
// ViewModels/MainViewModel.cs
public class MainViewModel : BaseViewModel, IDisposable
{
    // 바인딩 가능한 컬렉션
    public ObservableCollection<SatelliteViewModel> Satellites { get; } = new();
    public ObservableCollection<PassEvent>          Passes     { get; } = new();

    // 바인딩 가능한 프로퍼티 (변경 시 View 자동 갱신)
    private string _statusText = "Ready";
    public string StatusText
    {
        get => _statusText;
        set => SetField(ref _statusText, value);
    }

    // Command (버튼 바인딩)
    public ICommand PredictPassesCommand { get; }
}
```

### View — 선언적 UI

```xml
<!-- Views/MainWindow.axaml -->

<!-- 프로퍼티 바인딩: ViewModel.StatusText 변경 시 자동 갱신 -->
<TextBlock Text="{Binding StatusText}"/>

<!-- Command 바인딩: 클릭 → ViewModel.PredictPassesCommand.Execute() -->
<Button Command="{Binding PredictPassesCommand}" Content="패스 예측"/>

<!-- 컬렉션 바인딩: Passes 변경 시 DataGrid 자동 갱신 -->
<DataGrid ItemsSource="{Binding Passes}"/>

<!-- bool 직접 바인딩 (Avalonia 장점: Converter 불필요) -->
<Border IsVisible="{Binding HasSelection}"/>
```

---

## INotifyPropertyChanged 구현

ViewModel이 변경을 View에 알리는 메커니즘입니다.

```csharp
// ViewModels/BaseViewModel.cs
public abstract class BaseViewModel : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    protected void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    // 값이 바뀔 때만 알림 (성능 최적화)
    protected bool SetField<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        OnPropertyChanged(name);  // ← View에 알림
        return true;
    }
}
```

`[CallerMemberName]` 덕분에 프로퍼티 이름을 문자열로 하드코딩하지 않아도 됩니다:

```csharp
// 컴파일러가 자동으로 "StatusText" 문자열 삽입
set => SetField(ref _statusText, value);
//     ↑ CallerMemberName = "StatusText" (프로퍼티 이름 자동)
```

---

## ObservableCollection

```csharp
// 일반 List<T>: 추가/삭제를 View에 알리지 못함
List<PassEvent> list = new();
list.Add(pass);  // ← DataGrid 갱신 안 됨

// ObservableCollection<T>: INotifyCollectionChanged 구현
ObservableCollection<PassEvent> passes = new();
passes.Add(pass);  // ← DataGrid 자동 갱신!
```

---

## Command 패턴 (ICommand)

### WPF vs Avalonia RelayCommand 차이

```csharp
// ─── WPF RelayCommand ───────────────────────────────
public class RelayCommand : ICommand
{
    // WPF는 CommandManager가 자동으로 CanExecute 재평가
    public event EventHandler? CanExecuteChanged
    {
        add    => CommandManager.RequerySuggested += value;
        remove => CommandManager.RequerySuggested -= value;
    }
}

// ─── Avalonia RelayCommand ──────────────────────────
public class RelayCommand : ICommand
{
    // Avalonia는 CommandManager 없음 → 수동으로 알림
    public event EventHandler? CanExecuteChanged;

    public void RaiseCanExecuteChanged()
        => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}
```

```csharp
// ViewModel에서 사용
_predictCommand = new RelayCommand(
    execute:    _ => PredictPasses(),
    canExecute: _ => SelectedSatellite != null  // 위성 선택 시만 활성화
);

// 선택 변경 시 버튼 상태 갱신
public SatelliteViewModel? SelectedSatellite
{
    set
    {
        SetField(ref _selectedSat, value);
        OnPropertyChanged(nameof(HasSelection));
        _predictCommand.RaiseCanExecuteChanged();  // ← 버튼 활성화/비활성화
    }
}
```

---

## DispatcherTimer (실시간 업데이트)

```csharp
// Avalonia: Avalonia.Threading.DispatcherTimer (WPF와 API 동일)
var timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
timer.Tick += (_, _) =>
{
    CurrentTime = DateTime.Now;
    foreach (var sat in Satellites)
        sat.UpdatePosition(_engine);  // C++ 엔진 호출 (P/Invoke)
};
timer.Start();
```

`DispatcherTimer`는 UI 스레드에서 콜백을 실행합니다.
→ `ObservableCollection` 수정이 안전 (별도의 `InvokeAsync` 불필요)

---

## WPF vs Avalonia MVVM 비교

| 항목 | WPF | Avalonia |
|------|-----|----------|
| `INotifyPropertyChanged` | 동일 | 동일 |
| `ObservableCollection` | 동일 | 동일 |
| `ICommand` | 동일 | 동일 |
| `DispatcherTimer` | `System.Windows.Threading` | `Avalonia.Threading` |
| `Visibility` 바인딩 | BoolToVisibilityConverter 필요 | `IsVisible="{Binding bool}"` 직접 |
| `CommandManager` | 있음 (자동 재평가) | 없음 (수동 `RaiseCanExecuteChanged`) |
| DataTrigger | `<DataTrigger>` | CSS 셀렉터 or 계산 프로퍼티 |
| 파일 대화상자 | `Win32.OpenFileDialog` | `StorageProvider.OpenFilePickerAsync` |
| Models / ViewModels / Services | 완전히 동일 (재사용 가능) | ← 동일 |

---

## MVVM의 장점 (포트폴리오 설명 시)

1. **테스트 가능성**: ViewModel은 UI 없이 유닛 테스트 가능
2. **프레임워크 교체 용이**: WPF → Avalonia 전환 시 ViewModel 재사용 (이 프로젝트에서 실증)
3. **관심사 분리**: 디자이너(XAML) / 개발자(ViewModel) 분업 가능
4. **선언적 바인딩**: 코드비하인드 최소화, UI-데이터 동기화 자동
