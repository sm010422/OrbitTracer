# C++ / C# 상호운용 (P/Invoke)

## 왜 두 언어를 혼용하는가?

| 언어 | 강점 | 이 프로젝트 역할 |
|------|------|----------------|
| C++ | 연산 성능, 수치 계산 생태계 | SGP4 궤도 계산 엔진 |
| C# | 생산성, 크로스플랫폼 UI, 메모리 안전성 | Avalonia UI, 서비스 계층 |

이런 구조를 **네이티브 연동(Native Interop)** 이라 합니다.
실무 사례: Python NumPy (C 코어), TensorFlow (C++ 코어 + Python API)

---

## 1. C++ 쪽: extern "C" API 설계

C++의 **이름 맹글링(name mangling)** 때문에 C#이 직접 C++ 심볼을 찾을 수 없습니다.

```cpp
// C++에서 컴파일하면 SGP4::propagate → _ZN9SGP44propagateEd 같은 이름으로 바뀜
// extern "C"로 감싸면 C 호환 이름 유지
```

```cpp
// OrbitEngineAPI.h
#ifdef _WIN32
  #define ORBIT_API __declspec(dllexport)   // Windows DLL 심볼 내보내기
#else
  #define ORBIT_API __attribute__((visibility("default")))  // macOS/Linux
#endif

extern "C" {
    ORBIT_API int  orbit_init(const char* name, const char* line1, const char* line2);
    ORBIT_API void orbit_free(int handle);
    ORBIT_API int  orbit_get_position(int handle, double unix_ts, GeoPositionC* out);
    ORBIT_API int  orbit_predict_passes(int handle, double gs_lat, double gs_lon,
                                         double gs_alt, double start_unix,
                                         double duration_sec, double min_elev_deg,
                                         PassEventC* events, int max_passes);
}
```

### 왜 핸들(정수) 방식인가?

C#에서 C++ 포인터를 직접 다루면 메모리 안전성이 깨집니다.
대신 **정수 핸들**을 발급하고 C++ 내부 맵에서 실제 객체를 관리합니다.

```cpp
// OrbitEngineAPI.cpp
std::unordered_map<int, std::shared_ptr<SGP4>> g_sats;
std::mutex g_mutex;
int g_nextHandle = 1;

int orbit_init(...) {
    auto sgp4 = std::make_shared<SGP4>(tle);
    std::lock_guard lock(g_mutex);
    int h = g_nextHandle++;
    g_sats[h] = sgp4;
    return h;   // C#에는 정수만 노출
}
```

---

## 2. C# 쪽: P/Invoke 선언

```csharp
// 구조체: C++의 메모리 레이아웃을 명시
[StructLayout(LayoutKind.Sequential)]
public struct GeoPositionC
{
    public double Lat, Lon, Alt;  // double 3개 = 24바이트
}

[StructLayout(LayoutKind.Sequential)]
public struct PassEventC
{
    public double AosUnix, LosUnix, MaxElev, MaxElevUnix, AosAz, LosAz;
}

// P/Invoke 선언
public static class OrbitEngineNative
{
    private const string DllName = "OrbitEngine";
    // → Windows: OrbitEngine.dll
    // → macOS:   libOrbitEngine.dylib
    // → Linux:   libOrbitEngine.so

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int orbit_init(string name, string line1, string line2);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int orbit_get_position(
        int handle, double unix_ts, out GeoPositionC pos);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int orbit_predict_passes(
        int handle,
        double gs_lat, double gs_lon, double gs_alt,
        double start_unix, double duration_sec, double min_elev_deg,
        [Out] PassEventC[] events, int max_passes);
}
```

### 타입 매핑 규칙

| C++ | C# | 주의 |
|-----|----|------|
| `int` | `int` | |
| `double` | `double` | |
| `const char*` | `string` | 자동 UTF-8 마샬링 |
| `SomeStruct*` (출력) | `out SomeStruct` | `out` 키워드 |
| `T* arr, int n` (배열 출력) | `[Out] T[] arr, int n` | |
| `void` | `void` | |
| 호출 규약 | `CallingConvention.Cdecl` | C 기본값 |

---

## 3. 서비스 래퍼: IDisposable 패턴

```csharp
public class OrbitEngineService : IDisposable
{
    private readonly List<int> _handles = new();
    private bool _disposed;

    public int LoadSatellite(string name, string l1, string l2)
    {
        int h = OrbitEngineNative.orbit_init(name, l1, l2);
        if (h >= 0) _handles.Add(h);
        return h;
    }

    public (double lat, double lon, double alt)? GetPosition(int handle, DateTime? at = null)
    {
        double unix = (at?.ToUniversalTime() ?? DateTime.UtcNow - DateTime.UnixEpoch).TotalSeconds;
        int r = OrbitEngineNative.orbit_get_position(handle, unix, out GeoPositionC pos);
        return r == 0 ? (pos.Lat, pos.Lon, pos.Alt) : null;
    }

    public void Dispose()
    {
        if (_disposed) return;
        foreach (var h in _handles)
            OrbitEngineNative.orbit_free(h);  // C++ 측 shared_ptr 해제
        _handles.Clear();
        _disposed = true;
    }
}
```

C# `using` 블록으로 자동 해제:
```csharp
using var engine = new OrbitEngineService();
int h = engine.LoadSatellite("ISS", l1, l2);
// ... 사용 ...
// 블록 종료 → Dispose() → orbit_free() → C++ shared_ptr 소멸
```

---

## 4. 스레드 안전성

Avalonia UI는 여러 스레드를 사용합니다:
- **UI 스레드**: DispatcherTimer, 이벤트 핸들러
- **ThreadPool**: `async/await` 내부 연속

C++ 엔진은 `std::mutex`로 보호합니다:
```cpp
int orbit_get_position(int handle, double unix_ts, GeoPositionC* out) {
    std::lock_guard<std::mutex> lock(g_mutex);  // ← 스레드 안전
    auto it = g_sats.find(handle);
    ...
}
```

C# 쪽에서 UI 스레드 복귀:
```csharp
// 백그라운드 작업에서 UI 컬렉션 수정 시
await Dispatcher.UIThread.InvokeAsync(() => {
    Satellites.Add(new SatelliteViewModel(...));
});
```

---

## 5. 플랫폼별 빌드 및 배치

### macOS

```bash
cd OrbitEngine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# → build/libOrbitEngine.dylib

# Avalonia 앱 출력 폴더에 복사
cp build/libOrbitEngine.dylib \
   ../OrbitTracer.Avalonia/bin/Debug/net8.0/
```

### Windows

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
# → build/Release/OrbitEngine.dll
```

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# → build/libOrbitEngine.so
```

P/Invoke는 OS별로 자동으로 올바른 확장자를 찾습니다.
