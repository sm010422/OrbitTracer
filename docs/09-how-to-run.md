# 실행 가이드 (macOS)

> 현재 환경 기준: macOS 26.4, .NET 10, CMake 4.2.3

---

## 한눈에 보기

```
[1단계] C++ 엔진 빌드  →  libOrbitEngine.dylib 생성
[2단계] dylib 복사     →  앱 출력 폴더에 배치
[3단계] dotnet run     →  Avalonia 창 실행
```

---

## Step 1 — C++ 엔진 빌드

```bash
cd /Users/parksangmin/coding/c#/OrbitTracer/OrbitEngine

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

성공하면 이렇게 출력됩니다:
```
[100%] Linking CXX shared library libOrbitEngine.dylib
[100%] Built target OrbitEngine
```

확인:
```bash
ls build/libOrbitEngine.dylib
# → build/libOrbitEngine.dylib 있으면 정상
```

---

## Step 2 — dylib 복사

```bash
cp /Users/parksangmin/coding/c#/OrbitTracer/OrbitEngine/build/libOrbitEngine.dylib \
   /Users/parksangmin/coding/c#/OrbitTracer/OrbitTracer.Avalonia/bin/Debug/net10.0/
```

> **왜 복사해야 하나?**
> .NET 앱은 실행 파일과 같은 폴더에서 네이티브 라이브러리를 찾습니다.
> `dotnet run`은 `bin/Debug/net10.0/` 폴더에서 실행되므로 거기에 dylib가 있어야 합니다.

---

## Step 3 — 앱 실행

```bash
cd /Users/parksangmin/coding/c#/OrbitTracer/OrbitTracer.Avalonia
dotnet run
```

Avalonia 창이 뜨면 성공입니다.

---

## 자주 쓰는 전체 흐름 (한 번에 복붙)

```bash
# 엔진 빌드 + 복사 + 실행
cd /Users/parksangmin/coding/c#/OrbitTracer/OrbitEngine && \
cmake -B build -DCMAKE_BUILD_TYPE=Release && \
cmake --build build && \
cp build/libOrbitEngine.dylib \
   ../OrbitTracer.Avalonia/bin/Debug/net10.0/ && \
cd ../OrbitTracer.Avalonia && \
dotnet run
```

---

## 앱 사용법

창이 열리면:

| 동작 | 방법 |
|------|------|
| 기본 위성 (ISS) 로드 | 앱 시작 시 자동 로드 |
| 최신 TLE 가져오기 | **"ISS 가져오기"** 버튼 클릭 |
| TLE 파일 직접 열기 | **"TLE 파일 열기"** 버튼 → `.tle` / `.txt` 파일 선택 |
| 위성 선택 | 왼쪽 리스트에서 위성 클릭 |
| 지상국 변경 | 왼쪽 하단 드롭다운 (서울 / 대전 / 세종) |
| 패스 예측 | 위성 선택 후 **"패스 예측 (48h)"** 버튼 |
| 위성 제거 | **"위성 제거"** 버튼 |

---

## 트러블슈팅

### `Unable to load DLL 'OrbitEngine'`

```
System.DllNotFoundException: Unable to load DLL 'OrbitEngine'
```

dylib가 없거나 위치가 잘못된 것입니다.

```bash
# dylib 위치 확인
ls /Users/parksangmin/coding/c#/OrbitTracer/OrbitTracer.Avalonia/bin/Debug/net10.0/libOrbitEngine.dylib

# 없으면 Step 1~2 다시 실행
```

---

### macOS 보안 경고 ("개발자를 확인할 수 없음")

```bash
xattr -d com.apple.quarantine \
  /Users/parksangmin/coding/c#/OrbitTracer/OrbitTracer.Avalonia/bin/Debug/net10.0/libOrbitEngine.dylib
```

---

### NU1900 경고 (NuGet 캐시 권한)

```
warning NU1900: Access to the path '...' is denied.
```

빌드/실행에는 영향 없습니다. 무시해도 됩니다.
(원인: 이전에 `sudo dotnet` 실행으로 캐시 폴더 소유권이 root로 바뀐 것)

---

### `dotnet run` 후 창이 안 뜸

빌드는 성공했지만 창이 안 보이면, Dock이나 앱 전환(Cmd+Tab)에서 확인해보세요.
Avalonia 앱이 백그라운드로 실행된 경우가 있습니다.

---

### C++ 빌드 오류 (`cmake --build` 실패)

```bash
# build 폴더 초기화 후 재시도
cd /Users/parksangmin/coding/c#/OrbitTracer/OrbitEngine
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## .csproj 타겟 프레임워크 확인

이 프로젝트는 **net10.0** 으로 설정되어 있습니다.

```bash
grep TargetFramework /Users/parksangmin/coding/c#/OrbitTracer/OrbitTracer.Avalonia/OrbitTracer.Avalonia.csproj
# → <TargetFramework>net10.0</TargetFramework>
```

설치된 .NET 버전 확인:
```bash
dotnet --version   # 10.0.x 이어야 함
```

만약 .NET 버전이 다르면 csproj의 `net10.0`을 설치된 버전에 맞게 수정하세요:
- .NET 9 설치 → `net9.0`
- .NET 8 설치 → `net8.0`
