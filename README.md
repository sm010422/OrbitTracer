# 🛰️ OrbitTracer

> 위성 궤도 추적 및 지상국 패스 예측 시뮬레이터

C++로 구현한 SGP4 궤도 계산 엔진과 C# WPF 기반의 실시간 모니터링 GUI를 결합한 위성 추적 데스크탑 애플리케이션입니다.

---

## 📌 주요 기능

- **SGP4 궤도 계산** — TLE 데이터를 기반으로 위성의 실시간 위치(위경도, 고도) 계산
- **지상국 패스 예측** — AOS(Acquisition of Signal) / LOS(Loss of Signal) 시간대 자동 계산
- **가시성 분석** — 특정 지상국 기준 Elevation / Azimuth 계산
- **실시간 모니터링 GUI** — 위성 궤적 지도 표시 및 텔레메트리 데이터 시각화
- **이상 상태 알림** — 임계값 초과 시 경고 알림

---

## 🛠️ 기술 스택

| 영역 | 기술 |
|------|------|
| 궤도 계산 엔진 | C++17, SGP4/SDP4 알고리즘 |
| GUI 프레임워크 | C# / WPF (.NET 6) |
| 빌드 시스템 | CMake (C++), MSBuild (C#) |
| 위성 데이터 | [CelesTrak](https://celestrak.org) TLE 공개 데이터 |
| 형상 관리 | Git / GitHub |

---

## 📂 프로젝트 구조

```
OrbitTracer/
├── OrbitEngine/          # C++ 궤도 계산 엔진
│   ├── src/
│   │   ├── sgp4/         # SGP4 알고리즘 구현
│   │   ├── tle_parser/   # TLE 파싱 모듈
│   │   └── pass_predictor/ # 패스 예측 모듈
│   ├── include/
│   └── CMakeLists.txt
│
├── OrbitTracer.UI/       # C# WPF GUI
│   ├── Views/            # XAML 화면 구성
│   ├── ViewModels/       # MVVM 패턴
│   ├── Models/           # 데이터 모델
│   └── Services/         # 엔진 연동 서비스
│
├── data/                 # TLE 데이터 저장 경로 (gitignore)
├── docs/                 # 문서 및 스크린샷
├── .gitignore
└── README.md
```

---

## ⚙️ 빌드 및 실행

### 사전 요구 사항

- Visual Studio 2022 이상
- CMake 3.20 이상
- .NET 6 SDK

### C++ 엔진 빌드

```bash
cd OrbitEngine
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### C# GUI 실행

```bash
cd OrbitTracer.UI
dotnet run
```

---

## 🔭 핵심 알고리즘

### SGP4 (Simplified General Perturbations 4)

위성 궤도 예측의 국제 표준 알고리즘으로, TLE 데이터를 입력으로 받아 특정 시각의 위성 위치와 속도를 계산합니다.

```
입력: TLE (Two-Line Element Set)
출력: ECI 좌표 (위치 벡터, 속도 벡터)
      → ECEF 변환 → 위경도/고도 변환
```

### 패스 예측 (AOS / LOS)

지상국의 위경도 좌표를 기준으로 위성이 지평선 위로 올라오는 시간(AOS)과 내려가는 시간(LOS)을 예측합니다.

```
조건: Elevation > 5° (최소 통신 가능 앙각)
출력: AOS 시각, LOS 시각, 최대 앙각(Max Elevation)
```

---

## 📡 TLE 데이터 받는 법

CelesTrak에서 공개 TLE를 무료로 제공합니다.

```bash
# 예시: 아리랑 위성 TLE 다운로드
curl -o data/satellites.tle https://celestrak.org/SOCRATES/query.php
```

또는 [CelesTrak 웹사이트](https://celestrak.org/SOCRATES/)에서 직접 다운로드 가능합니다.

---

## 📸 스크린샷

> 추후 추가 예정

---

## 📖 참고 자료

- [Vallado, D.A. - Fundamentals of Astrodynamics and Applications](https://www.celestrak.com/software/vallado-sw.asp)
- [CelesTrak - TLE Data](https://celestrak.org)
- [AGI STK - SGP4 Reference](https://help.agi.com/stk/)

---

## 📝 라이선스

MIT License
