# WPF → Avalonia UI 마이그레이션 노트

## 왜 Avalonia인가?

| 항목 | WPF | Avalonia |
|------|-----|----------|
| 플랫폼 | Windows 전용 | Windows / macOS / Linux |
| 렌더러 | DirectX | Skia (GPU 가속) |
| XAML 문법 | WPF 방언 | WPF 유사 + 개선 |
| 라이선스 | .NET 포함 | MIT 오픈소스 |
| 활발한 개발 | 유지보수 모드 | 활발히 개발 중 |

**학습 관점**: WPF를 배우면 Avalonia도 80~90% 그대로 적용됩니다.
**이 프로젝트**: Models / ViewModels / Services 코드를 **한 줄도 수정 없이** 재사용했습니다.

---

## 프로젝트 구조 비교

```
OrbitTracer.UI/          (WPF, Windows 전용)
├── App.xaml             ← StartupUri="Views/MainWindow.xaml"
├── App.xaml.cs
└── Views/MainWindow.xaml

OrbitTracer.Avalonia/    (Avalonia, 크로스플랫폼)
├── Program.cs           ← AppBuilder 진입점 (NEW)
├── App.axaml            ← 전역 스타일/리소스
├── App.axaml.cs         ← OnFrameworkInitializationCompleted
└── Views/MainWindow.axaml
```

---

## 변경 사항 상세

### 1. 앱 진입점

```csharp
// ─── WPF: App.xaml의 StartupUri 속성으로 처리 ───
// App.xaml.cs
public partial class App : Application { }  // 거의 비어있음

// ─── Avalonia: Program.cs (새 파일) ─────────────
class Program
{
    [STAThread]
    public static void Main(string[] args)
        => BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);

    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()    // macOS/Windows/Linux 자동 감지
            .WithInterFont()        // Inter 폰트 번들
            .LogToTrace();
}

// App.axaml.cs
public partial class App : Application
{
    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            desktop.MainWindow = new MainWindow();  // 직접 생성
        base.OnFrameworkInitializationCompleted();
    }
}
```

### 2. XAML 네임스페이스

```xml
<!-- WPF -->
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">

<!-- Avalonia -->
<Window xmlns="https://github.com/avaloniaui"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        x:DataType="vm:MainViewModel">  ← 컴파일드 바인딩 타입 명시
```

### 3. 테마 및 스타일

```xml
<!-- WPF: App.xaml -->
<Application.Resources>
    <Style x:Key="ActionButton" TargetType="Button">
        <Setter Property="Template">
            <Setter.Value>
                <ControlTemplate>
                    <Border Background="{TemplateBinding Background}" CornerRadius="3">
                        ...
                    </Border>
                </ControlTemplate>
            </Setter.Value>
        </Setter>
    </Style>
</Application.Resources>

<!-- Avalonia: App.axaml -->
<Application.Styles>
    <FluentTheme />
    <StyleInclude Source="avares://Avalonia.Controls.DataGrid/Themes/Fluent.xaml"/>
</Application.Styles>
```

```xml
<!-- Avalonia: Window 내 인라인 스타일 (CSS-like 셀렉터) -->
<Window.Styles>
    <Style Selector="Button.action">
        <Setter Property="Background" Value="#0F3460"/>
        <Setter Property="CornerRadius" Value="3"/>
    </Style>
    <Style Selector="Button.action:pointerover /template/ ContentPresenter">
        <Setter Property="Background" Value="#1a4a80"/>
    </Style>
</Window.Styles>

<!-- 사용: Classes="action" -->
<Button Classes="action" Content="클릭"/>
```

### 4. ListBox 아이템 스타일

```xml
<!-- WPF: ItemContainerStyle + Triggers -->
<ListBox.ItemContainerStyle>
    <Style TargetType="ListBoxItem">
        <Style.Triggers>
            <Trigger Property="IsSelected" Value="True">
                <Setter Property="Background" Value="#40E94560"/>
            </Trigger>
        </Style.Triggers>
    </Style>
</ListBox.ItemContainerStyle>

<!-- Avalonia: CSS 셀렉터 -->
<ListBox.Styles>
    <Style Selector="ListBoxItem:selected /template/ ContentPresenter">
        <Setter Property="Background" Value="#40E94560"/>
    </Style>
    <Style Selector="ListBoxItem:pointerover /template/ ContentPresenter">
        <Setter Property="Background" Value="#200F3460"/>
    </Style>
</ListBox.Styles>
```

### 5. Visibility vs IsVisible

```xml
<!-- WPF: Visibility enum + Converter 필요 -->
<Border Visibility="{Binding HasSelection, Converter={StaticResource BoolToVisibility}}"/>

<!-- Avalonia: bool 직접 바인딩 (훨씬 간결) -->
<Border IsVisible="{Binding HasSelection}"/>
```

WPF에서는 `BoolToVisibilityConverter` 클래스를 만들어야 했지만
Avalonia는 `bool` → `IsVisible` 직접 바인딩을 지원합니다.

### 6. DataGrid

```xml
<!-- WPF -->
<DataGrid xmlns은 System.Windows.Controls에 포함>

<!-- Avalonia: 별도 패키지 + 테마 포함 필요 -->
<!-- csproj에 추가: Avalonia.Controls.DataGrid -->
<!-- App.axaml에: <StyleInclude Source="avares://Avalonia.Controls.DataGrid/Themes/Fluent.xaml"/> -->
```

### 7. 파일 대화상자

```csharp
// ─── WPF ────────────────────────────────────────────
var dlg = new Microsoft.Win32.OpenFileDialog
{
    Filter = "TLE Files (*.tle;*.txt)|*.tle;*.txt"
};
if (dlg.ShowDialog() == true)
    content = File.ReadAllText(dlg.FileName);

// ─── Avalonia ───────────────────────────────────────
// code-behind에서 (Window/TopLevel 접근 필요)
var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
{
    Title = "TLE 파일 열기",
    AllowMultiple = false,
    FileTypeFilter = new[]
    {
        new FilePickerFileType("TLE Files") { Patterns = new[] { "*.tle", "*.txt" } }
    }
});
if (files.Count > 0)
{
    await using var stream = await files[0].OpenReadAsync();
    using var reader = new StreamReader(stream);
    content = await reader.ReadToEndAsync();
}
```

**설계 결정**: Avalonia의 파일 선택은 `Window` 참조가 필요해서 code-behind에 두고,
ViewModel에는 `LoadTLEContent(string content)` 메서드만 노출했습니다.
→ ViewModel의 View 독립성 유지

### 8. MessageBox

```csharp
// WPF
MessageBox.Show("Error", "Title", MessageBoxButton.OK, MessageBoxImage.Warning);

// Avalonia: MsBox.Avalonia 패키지 (비동기) 또는 StatusText로 대체
// 이 프로젝트: StatusText 프로퍼티로 에러 메시지 표시 (더 자연스러운 UX)
StatusText = $"오류: {ex.Message}";
```

### 9. Canvas 마커 그리기 (code-behind)

```csharp
// WPF
var ellipse = new Ellipse { Width = 8, Height = 8 };
ellipse.Margin = new Thickness(x-4, y-4, 0, 0);  // Margin 트릭

// Avalonia: Canvas 첨부 속성 사용 (더 명확)
var ellipse = new Avalonia.Controls.Shapes.Ellipse { Width = 8, Height = 8 };
Canvas.SetLeft(ellipse, x - 4);
Canvas.SetTop (ellipse, y - 4);
MapCanvas.Children.Add(ellipse);
```

---

## 재사용된 코드 (변경 없음)

이 프로젝트에서 WPF → Avalonia 전환 시 **수정 없이 재사용**한 파일들:

| 파일 | 재사용 여부 | 이유 |
|------|------------|------|
| `Models/*.cs` | ✅ 100% 재사용 | 순수 C# 데이터, UI 의존 없음 |
| `Services/OrbitEngineService.cs` | ✅ 100% 재사용 | P/Invoke는 플랫폼 중립 |
| `Services/TLEFetchService.cs` | ✅ 100% 재사용 | `HttpClient`는 플랫폼 중립 |
| `ViewModels/BaseViewModel.cs` | ✅ 100% 재사용 | `INotifyPropertyChanged`는 공통 |
| `ViewModels/MainViewModel.cs` | 🔧 소폭 수정 | `DispatcherTimer` 네임스페이스, 파일 대화상자 제거 |
| `Views/MainWindow.axaml` | 🔄 재작성 | XAML 문법/스타일 차이 |
| `Views/MainWindow.axaml.cs` | 🔄 재작성 | Canvas 그리기, 파일 선택 방식 차이 |

**결론**: 비즈니스 로직(ViewModel)과 데이터(Model)는 UI 프레임워크와 완전히 독립적입니다.
MVVM의 핵심 이점이 실제로 증명된 사례입니다.

---

## Avalonia 앱 실행 방법 (macOS)

```bash
# 1. C++ 엔진 빌드
cd OrbitEngine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. dylib 복사
cp build/libOrbitEngine.dylib \
   ../OrbitTracer.Avalonia/bin/Debug/net8.0/

# 3. 앱 실행
cd ../OrbitTracer.Avalonia
dotnet run
```
