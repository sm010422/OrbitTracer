# 빌드 및 실행 가이드

## macOS (개발 환경)

### 사전 요구사항

```bash
# Homebrew가 없으면 먼저 설치
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# CMake 설치
brew install cmake

# .NET 8 SDK 설치
brew install --cask dotnet-sdk
# 또는 https://dotnet.microsoft.com/download 에서 다운로드
```

버전 확인:
```bash
cmake --version    # cmake version 3.20 이상
dotnet --version   # 8.0.x
```

---

### Step 1: C++ 엔진 빌드

```bash
cd OrbitEngine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 결과물 확인
ls build/
# libOrbitEngine.dylib  ← 이 파일이 필요
```

### Step 2: dylib를 앱 출력 폴더에 복사

```bash
# Debug 빌드용
cp OrbitEngine/build/libOrbitEngine.dylib \
   OrbitTracer.Avalonia/bin/Debug/net8.0/

# 또는 Release 빌드용
cp OrbitEngine/build/libOrbitEngine.dylib \
   OrbitTracer.Avalonia/bin/Release/net8.0/
```

### Step 3: Avalonia 앱 빌드 & 실행

```bash
cd OrbitTracer.Avalonia

# 패키지 복원 (첫 실행 시)
dotnet restore

# 빌드만
dotnet build

# 빌드 + 실행
dotnet run
```

---

## Windows

### 사전 요구사항

- Visual Studio 2022 (C++ 워크로드 포함)
- .NET 8 SDK
- CMake 3.20+

### 빌드 순서

```bash
# C++ 엔진
cd OrbitEngine
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
# → build/Release/OrbitEngine.dll

# DLL 복사
copy OrbitEngine\build\Release\OrbitEngine.dll ^
     OrbitTracer.Avalonia\bin\Debug\net8.0\

# C# 앱 실행
cd OrbitTracer.Avalonia
dotnet run

# WPF 버전 실행 (Windows 전용)
cd OrbitTracer.UI
dotnet run
```

---

## Linux

```bash
# 의존성 설치 (Ubuntu/Debian)
sudo apt install cmake build-essential

# C++ 엔진
cd OrbitEngine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# → build/libOrbitEngine.so

cp build/libOrbitEngine.so \
   ../OrbitTracer.Avalonia/bin/Debug/net8.0/

# 앱 실행
cd ../OrbitTracer.Avalonia
dotnet run
```

---

## TLE 데이터 준비

### 방법 1: 앱 내 버튼

실행 후 **"ISS 가져오기"** 버튼 클릭 → CelesTrak에서 자동 다운로드

### 방법 2: curl로 직접 다운로드

```bash
mkdir -p data

# 우주 정거장 (ISS 포함)
curl -o data/stations.tle https://celestrak.org/pub/TLE/stations.txt

# 기상위성 (NOAA, GOES 등)
curl -o data/weather.tle https://celestrak.org/pub/TLE/weather.txt

# 과학위성
curl -o data/science.tle https://celestrak.org/pub/TLE/science.txt

# 한국 위성 (아리랑 등)
curl -o data/korea.tle "https://celestrak.org/pub/TLE/catalog.txt"
```

### TLE 파일 형식

```
ISS (ZARYA)                               ← 이름 (선택)
1 25544U 98067A   24001.50000000  .00006089  00000+0  11294-3 0  9993
2 25544  51.6416 290.5000 0001234  80.0000 280.0000 15.49000000440000
```

- 3줄 형식: 이름 + Line1 + Line2
- 2줄 형식도 지원 (이름 없음)
- 각 라인은 69자 이상이어야 함

---

## 트러블슈팅

### "Unable to load DLL 'OrbitEngine'"

```
DllNotFoundException: Unable to load DLL 'OrbitEngine'
```

→ `libOrbitEngine.dylib` (macOS) 또는 `OrbitEngine.dll` (Windows)가
   실행 파일과 같은 폴더에 있는지 확인

```bash
ls OrbitTracer.Avalonia/bin/Debug/net8.0/
# libOrbitEngine.dylib 있어야 함
```

### macOS: "libOrbitEngine.dylib cannot be opened because the developer cannot be verified"

```bash
# 격리 속성 제거
xattr -d com.apple.quarantine libOrbitEngine.dylib
```

### dotnet restore 느림

```bash
# NuGet 패키지 캐시 직접 사용 (오프라인)
dotnet restore --no-cache  # 역설적으로 때로는 더 빠름
```

### TLE 파싱 실패

- 파일 인코딩이 UTF-8인지 확인 (Windows에서 생성한 파일은 BOM 포함일 수 있음)
- 각 라인 길이가 69자 이상인지 확인
- 줄바꿈이 `\r\n` (Windows)이어도 파서가 자동 처리

### 위성 위치가 이상하게 표시됨

- TLE 에폭이 오래된 경우 (수주 이상) 오차가 커집니다
- "ISS 가져오기"로 최신 TLE를 다운로드하세요
- SGP4는 에폭 기준 ±7일 이내에서 가장 정확합니다
