# 학습 로드맵 & 개념 정리

이 프로젝트를 만들면서 필요한 지식과 학습 순서를 정리합니다.

---

## 전체 학습 지도

```
[사전 지식]
  ├── C++ 기초 (포인터, 클래스, 템플릿)
  └── C# 기초 (클래스, 인터페이스, async/await)
         │
         ▼
[Phase 1: 수치 계산]
  ├── 케플러 궤도 요소 이해
  ├── 케플러 방정식 & Newton-Raphson
  └── TLE 형식 파싱
         │
         ▼
[Phase 2: 좌표 변환]
  ├── ECI 좌표계
  ├── GMST & ECEF 변환
  └── Geodetic (WGS84 Bowring)
         │
         ▼
[Phase 3: SGP4 구현]
  ├── J2/J4 섭동 초기화
  ├── 세속 항 전파
  └── 단주기 섭동 보정
         │
         ▼
[Phase 4: C++ 엔진]
  ├── 클래스 설계 (SGP4, TLEParser, PassPredictor)
  ├── CMake 빌드 시스템
  └── Shared Library 빌드
         │
         ▼
[Phase 5: C# 연동]
  ├── extern "C" API 설계
  ├── P/Invoke & StructLayout
  └── IDisposable 패턴
         │
         ▼
[Phase 6: MVVM & UI]
  ├── INotifyPropertyChanged
  ├── ObservableCollection & ICommand
  ├── WPF XAML 기초
  └── Avalonia 마이그레이션
```

---

## Phase 1: 케플러 궤도 & TLE

### 핵심 개념

| 개념 | 설명 | 학습 포인트 |
|------|------|------------|
| 궤도 요소 6개 | a, e, i, Ω, ω, M | 궤도의 크기/형태/방향/위치 |
| TLE 형식 | 2줄 69자 고정형 | 가정소수점 파싱 방식 |
| 에폭 | YYDDD.DDDDDDDD | Julian Date 변환 |
| 평균 운동 | rev/day → rad/min | 단위 변환 중요 |

### 배운 것

```
TLE의 에폭 파싱:
  "24001.50000000"
  → 2024년의 1.50000번째 날
  → 2024-01-01 12:00:00 UTC
  → Julian Date 2460311.000
  → Unix timestamp 계산
```

---

## Phase 2: 좌표 변환

### 변환 체인

```
SGP4 출력 (ECI, km)
    ↓  GMST 회전 (지구 자전 보정)
ECEF (지구 고정계, km)
    ↓  Bowring 반복법 (WGS84 타원체)
Geodetic (위도°, 경도°, 고도 km)
    ↓  SEZ 변환 (지상국 기준)
AltAz (앙각°, 방위각°, 거리 km)
```

### 핵심 수식

**GMST (그리니치 평균 항성시)**:
```
T = (JD - 2451545.0) / 36525  ← J2000.0 기준 율리우스 세기
θ_GMST = 67310.54841 + (876600*3600 + 8640184.812866)*T + ...
```

**Bowring 반복 위도**:
```
φ₀ = atan2(z, p*(1-e²))
φₙ₊₁ = atan2(z + e²·N·sin(φₙ), p)    (N = a / √(1-e²sin²φ))
```

---

## Phase 3-4: C++ 설계

### 배운 C++ 패턴

```cpp
// 1. RAII (Resource Acquisition Is Initialization)
//    생성자에서 획득, 소멸자에서 해제
class SGP4 {
public:
    SGP4(const TLEData& tle) { initialize(tle); }  // 생성 시 초기화
    ~SGP4() = default;  // 별도 해제 불필요 (value semantics)
};

// 2. shared_ptr로 공유 소유권
std::unordered_map<int, std::shared_ptr<SGP4>> g_sats;
// 핸들 삭제 시 마지막 shared_ptr 소멸 → SGP4 자동 해제

// 3. lock_guard (RAII 스타일 뮤텍스)
{
    std::lock_guard<std::mutex> lock(g_mutex);  // 생성 시 lock
    // ... 임계구역 ...
}  // 소멸 시 자동 unlock

// 4. CMake shared library 타겟
add_library(OrbitEngine SHARED ${SOURCES})
target_include_directories(OrbitEngine PUBLIC include)
```

---

## Phase 5: P/Invoke 핵심 개념

### 마샬링(Marshaling)

C#의 타입을 C ABI 타입으로 변환하는 과정입니다.

```csharp
// string → const char* (자동 마샬링)
[DllImport("OrbitEngine")]
static extern int orbit_init(string name, string l1, string l2);
// string은 자동으로 UTF-8 C 문자열로 변환됨

// 구조체 → C 구조체 (명시적 레이아웃 필요)
[StructLayout(LayoutKind.Sequential)]
struct GeoPositionC {
    public double Lat, Lon, Alt;
    // double 3개, 각 8바이트, 총 24바이트 → C의 double[3]과 일치
}

// 배열 → 포인터 (Out 한정자)
[DllImport("OrbitEngine")]
static extern int orbit_predict_passes(
    int handle,
    ...,
    [Out] PassEventC[] events,  // C: PassEventC* events
    int max_passes);
```

### IDisposable 패턴의 중요성

```csharp
// 잘못된 방식 (누수 가능)
int h1 = OrbitEngineNative.orbit_init(...);
int h2 = OrbitEngineNative.orbit_init(...);
// h1, h2 해제를 잊으면 C++ 메모리 누수

// 올바른 방식 (OrbitEngineService)
using var engine = new OrbitEngineService();
int h = engine.LoadSatellite(...);
// using 블록 종료 → Dispose() → 모든 핸들 자동 해제
```

---

## Phase 6: MVVM 패턴

### 바인딩의 작동 원리

```
1. View는 DataContext의 프로퍼티를 바인딩 등록
2. ViewModel이 OnPropertyChanged("StatusText") 발생
3. WPF/Avalonia 바인딩 엔진이 이벤트를 감지
4. View의 해당 요소를 자동으로 재렌더링
```

```csharp
// OnPropertyChanged 발생 → {Binding StatusText}를 가진 모든 TextBlock 갱신
StatusText = "새 메시지";
```

### ObservableCollection vs List

```csharp
List<PassEvent> list = new();
// list.Add(x) → UI 알림 없음

ObservableCollection<PassEvent> oc = new();
// oc.Add(x) → CollectionChanged 이벤트 → DataGrid 자동 행 추가
// oc.Remove(x) → CollectionChanged 이벤트 → DataGrid 자동 행 제거
// oc.Clear() → CollectionChanged 이벤트 → DataGrid 자동 전체 삭제
```

---

## 이 프로젝트로 경험한 실전 문제들

### 문제 1: NuGet 캐시 권한 오류

```
Access denied: /Users/.../.local/share/NuGet/http-cache/...
```

**원인**: 이전 `sudo dotnet` 실행으로 캐시 폴더가 root 소유권이 됨

**해결**: `nuget.config`에서 캐시 경로를 사용자 디렉터리로 재지정
```xml
<configuration>
  <config>
    <add key="globalPackagesFolder" value="/tmp/nuget-packages"/>
  </config>
</configuration>
```

### 문제 2: Avalonia DataGrid 스타일 누락

**원인**: DataGrid는 별도 패키지(`Avalonia.Controls.DataGrid`)이며, 테마도 별도로 인클루드해야 함

**해결**:
```xml
<!-- App.axaml -->
<StyleInclude Source="avares://Avalonia.Controls.DataGrid/Themes/Fluent.xaml"/>
```

### 문제 3: Avalonia 컴파일드 바인딩과 DataTemplate

**원인**: `AvaloniaUseCompiledBindingsByDefault=true` 설정 시 DataTemplate에 `x:DataType` 필수

**해결**:
```xml
<DataTemplate x:DataType="vm:SatelliteViewModel">
    <TextBlock Text="{Binding Name}"/>  ← 컴파일 타임 타입 검사
</DataTemplate>
```

### 문제 4: macOS에서 Canvas 크기가 0

**원인**: `OnOpened` 이전에 `Bounds.Width`가 0

**해결**: `OnOpened` 이후에 DrawMap() 호출, 또는 `SizeChanged` 이벤트 사용
```csharp
protected override void OnOpened(EventArgs e)
{
    base.OnOpened(e);
    DrawMap();  // 창이 열린 후 크기가 확정된 시점에 호출
}
```

---

## 다음 단계 (확장 아이디어)

| 기능 | 난이도 | 학습 내용 |
|------|--------|----------|
| 실제 세계 지도 이미지 오버레이 | ⭐⭐ | Avalonia Image, 이미지 좌표 변환 |
| 궤도 자취 (지난 N분 궤적) | ⭐⭐ | Polyline, 시간 범위 전파 |
| 앙각 vs 시간 그래프 | ⭐⭐⭐ | OxyPlot Avalonia, 차트 라이브러리 |
| 알림 시스템 (AOS 임박 경고) | ⭐⭐ | `BackgroundService`, OS 알림 |
| SDP4 추가 (딥스페이스 위성) | ⭐⭐⭐⭐ | 태양/달 중력 섭동 모델 |
| 다중 지상국 동시 예측 | ⭐⭐ | 병렬 처리, `Task.WhenAll` |
