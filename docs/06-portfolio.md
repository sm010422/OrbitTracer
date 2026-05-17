# OrbitTracer — 포트폴리오 설명서

## 한 줄 소개

> C++17 SGP4 궤도 엔진 + C# Avalonia UI를 결합한 크로스플랫폼 실시간 위성 추적 데스크탑 앱

---

## 프로젝트 배경

항공우주 분야에 관심이 있어 실제 위성 추적 소프트웨어의 핵심 알고리즘인 **SGP4**를 직접 구현해보고자 시작했습니다.

**목표 두 가지:**
1. 복잡한 수치 알고리즘(SGP4)을 직접 이해하고 구현
2. C++의 연산 성능과 C#의 생산성을 결합하는 **크로스 언어 아키텍처** 경험

---

## 기술 스택

| 영역 | 기술 | 버전 |
|------|------|------|
| 궤도 계산 엔진 | C++17 | — |
| 빌드 시스템 | CMake | 3.20+ |
| UI 프레임워크 | Avalonia UI | 11.2 |
| 런타임 | .NET | 8.0 |
| 패턴 | MVVM | CommunityToolkit.Mvvm |
| 언어 간 연동 | P/Invoke | — |
| 위성 데이터 | CelesTrak TLE | 공개 데이터 |

---

## 기술적 도전과 해결책

### 도전 1 — SGP4 알고리즘 직접 구현

**문제**: 위성 궤도 계산은 지구 비구형성(J₂/J₄ 중력항), 대기 항력(BSTAR), 케플러 방정식 반복 풀이 등 수치 계산의 집약체입니다.

**해결**: Vallado의 참조 구현과 SPACETRACK Report #3 논문을 분석하여 C++17로 구현.

```
TLE 파싱 → 섭동 초기화 → 시간 전파
  → 케플러 방정식 (Newton-Raphson) → ECI 벡터
  → GMST 회전 → ECEF → Bowring 반복 → 위경도/고도
```

---

### 도전 2 — C++ / C# 크로스 언어 아키텍처

**문제**: C++ 객체를 C#에서 안전하게 제어하면서, 메모리 누수와 타입 불일치를 방지해야 합니다.

**해결**: `extern "C"` + 핸들 패턴 + `IDisposable`

```cpp
// C++: 정수 핸들로 추상화 (포인터 직접 노출 X)
extern "C" ORBIT_API int orbit_init(const char* name, const char* l1, const char* l2);
extern "C" ORBIT_API int orbit_get_position(int handle, double unix_ts, GeoPositionC* out);
```

```csharp
// C#: [StructLayout]으로 메모리 레이아웃 명시
[StructLayout(LayoutKind.Sequential)]
struct GeoPositionC { public double Lat, Lon, Alt; }

// IDisposable로 C++ 핸들 수명 관리
public class OrbitEngineService : IDisposable {
    public void Dispose() => _handles.ForEach(OrbitEngineNative.orbit_free);
}
```

---

### 도전 3 — WPF → Avalonia 크로스플랫폼 마이그레이션

**문제**: WPF는 Windows 전용이라 macOS 개발 환경에서 실행 불가.

**해결**: Avalonia UI로 마이그레이션. MVVM 패턴 덕분에 **Models / ViewModels / Services 코드를 한 줄도 수정하지 않고 재사용**.

```
수정된 것: App.axaml, MainWindow.axaml, MainWindow.axaml.cs (Views 계층만)
재사용된 것: 모든 Models, Services, ViewModels (비즈니스 로직 전체)
```

주요 차이점 해결:
- `BoolToVisibilityConverter` → `IsVisible="{Binding bool}"` 직접 바인딩
- `Win32.OpenFileDialog` → `StorageProvider.OpenFilePickerAsync()`
- `CommandManager` → `RaiseCanExecuteChanged()` 수동 호출
- WPF `DataTrigger` → Avalonia CSS-like 셀렉터 스타일

---

### 도전 4 — 실시간 패스 예측 성능

**문제**: AOS/LOS 시각을 초 단위로 스캔하면 48시간 × 위성 수 × (초당 계산)으로 너무 느립니다.

**해결**: 2단계 탐색 알고리즘

```
1단계: 15초 간격 coarse scan → O(duration/15)
2단계: 이분법 25회 → 교차 시각을 ~0.5초 정밀도로 수렴
→ 48시간 예측이 밀리초 내 완료
```

---

## 학습한 핵심 내용

| 영역 | 내용 |
|------|------|
| **수치 계산** | SGP4 섭동 이론, Newton-Raphson, 이분법 |
| **좌표 변환** | ECI → ECEF → Geodetic (WGS84 Bowring), GMST |
| **C++** | `shared_ptr`, `mutex`, `extern "C"`, CMake |
| **C# Interop** | `DllImport`, `StructLayout`, `CallingConvention` |
| **MVVM** | `INotifyPropertyChanged`, `ObservableCollection`, `ICommand` |
| **Avalonia** | Compiled Bindings, CSS 셀렉터 스타일, `StorageProvider` |
| **아키텍처** | 핸들 패턴, RAII, 계층 분리, 관심사 분리 |

---

## 프로젝트 구조

```
OrbitTracer/
├── OrbitEngine/           ← C++17 공유 라이브러리
│   ├── SGP4               ← 궤도 전파 (섭동 포함)
│   ├── TLEParser          ← TLE 파싱
│   ├── PassPredictor      ← AOS/LOS 예측
│   └── C API              ← P/Invoke 인터페이스
│
├── OrbitTracer.UI/        ← C# WPF (Windows 레퍼런스)
├── OrbitTracer.Avalonia/  ← C# Avalonia (크로스플랫폼)
│   ├── Models             ← 데이터
│   ├── Services           ← 엔진 연동 + TLE 다운로드
│   ├── ViewModels         ← MVVM 로직 + 실시간 타이머
│   └── Views              ← 다크테마 UI
│
└── docs/                  ← 학습 노트 & 포트폴리오
```

---

## 면접 예상 질문 대비

**Q. SGP4와 케플러 궤도의 차이는?**

케플러 궤도는 이상적인 2체 문제(지구-위성)만 고려합니다. SGP4는 지구 비구형성(J₂, J₄), 대기 항력(BSTAR)을 추가 모델링합니다. J₂ 효과만 해도 ISS의 RAAN이 하루에 약 7° 세차하는데, 이를 무시하면 몇 시간만 지나도 예측이 크게 틀려집니다.

**Q. P/Invoke에서 핸들 패턴을 쓴 이유는?**

C#에서 C++ 포인터를 직접 보유하면 GC가 객체를 이동시킬 때 포인터가 무효화될 수 있습니다. 정수 핸들을 발급하고 C++ 측 `std::unordered_map`에서 `shared_ptr`로 관리하면, C#은 숫자만 보유하고 실제 객체 수명은 C++이 완전히 통제합니다.

**Q. WPF → Avalonia 전환 시 얼마나 재사용했나?**

Models, Services, ViewModels(비즈니스 로직) 전체를 재사용했습니다. MVVM 패턴이 View와 ViewModel을 분리하기 때문에 Views 계층만 수정하면 됐습니다. 이 경험으로 MVVM의 실질적 이점(프레임워크 교체 용이성)을 직접 체감했습니다.

**Q. 실시간 업데이트에서 스레드 안전성을 어떻게 보장하는가?**

C++ 엔진은 `std::mutex`로 핸들 맵을 보호합니다. C# 쪽은 `DispatcherTimer`가 UI 스레드에서 콜백을 실행하므로 `ObservableCollection` 수정이 안전합니다. 백그라운드 TLE 다운로드는 `async/await`로 처리하고, UI 컬렉션 추가는 `Dispatcher.UIThread.InvokeAsync()`로 UI 스레드에 마샬링합니다.

**Q. 이분법을 패스 예측에 쓴 이유는?**

AOS/LOS는 앙각 함수의 영점(zero crossing)을 찾는 문제입니다. 함수가 연속이고 부호 변화가 보장되는 구간에서 이분법은 매 반복마다 오차 구간을 절반으로 줄입니다. 25회면 초기 구간(15초)을 4.5×10⁻⁷ 초까지 좁힐 수 있어 충분합니다. Newton법보다 구현이 단순하고 수렴이 보장됩니다.
