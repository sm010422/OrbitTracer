# 시스템 아키텍처

## 전체 구조

```
┌──────────────────────────────────────────────────────────────┐
│              OrbitTracer.Avalonia (C# / Avalonia UI)         │
│                                                              │
│  ┌──────────┐   Data Binding   ┌────────────┐               │
│  │  Views   │◄────────────────►│ ViewModels │               │
│  │ (.axaml) │   Command 패턴   │   (MVVM)   │               │
│  └──────────┘                  └─────┬──────┘               │
│                                      │ 호출                  │
│                               ┌──────▼──────┐               │
│                               │  Services   │               │
│                               │  (엔진 연동) │               │
│                               └──────┬──────┘               │
└──────────────────────────────────────┼──────────────────────┘
                                       │ P/Invoke
                                       ▼
┌──────────────────────────────────────────────────────────────┐
│              OrbitEngine (C++17 Shared Library)              │
│                                                              │
│  ┌─────────────┐   ┌──────────────┐   ┌───────────────────┐ │
│  │  TLEParser  │──►│     SGP4     │──►│  PassPredictor    │ │
│  │  (TLE 파싱) │   │  (궤도 계산)  │   │  (AOS/LOS 예측)   │ │
│  └─────────────┘   └──────────────┘   └───────────────────┘ │
│                           │                                  │
│                    ┌──────▼──────┐                           │
│                    │  C API      │ ← extern "C" + ORBIT_API  │
│                    │  (핸들 기반) │                           │
│                    └─────────────┘                           │
└──────────────────────────────────────────────────────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │  CelesTrak TLE 데이터  │
              │  (공개 위성 궤도 데이터) │
              └────────────────────────┘
```

## 프로젝트 구성

```
OrbitTracer/
├── OrbitEngine/               ← C++17 공유 라이브러리
│   ├── include/
│   │   ├── SGP4.h             ← TLEData, EciState, SGP4 클래스
│   │   ├── TLEParser.h
│   │   ├── PassPredictor.h    ← GroundStation, PassEvent, AltAz
│   │   └── OrbitEngineAPI.h   ← C API (P/Invoke 진입점)
│   ├── src/
│   │   ├── sgp4/SGP4.cpp
│   │   ├── tle_parser/TLEParser.cpp
│   │   ├── pass_predictor/PassPredictor.cpp
│   │   └── OrbitEngineAPI.cpp ← 핸들 레지스트리 (mutex 보호)
│   └── CMakeLists.txt
│
├── OrbitTracer.UI/            ← C# WPF (Windows 전용, 레퍼런스용)
│
├── OrbitTracer.Avalonia/      ← C# Avalonia (macOS/Windows/Linux)
│   ├── Program.cs             ← AppBuilder 진입점
│   ├── App.axaml              ← 전역 테마/리소스
│   ├── Models/
│   ├── Services/              ← P/Invoke, TLE 다운로드
│   ├── ViewModels/            ← MVVM 로직
│   └── Views/                 ← XAML UI
│
└── docs/                      ← 학습 노트 & 포트폴리오
```

## 계층별 책임

| 계층 | 언어 | 책임 |
|------|------|------|
| **Engine** | C++17 | SGP4 전파, 좌표 변환, 패스 예측 |
| **C API** | C | P/Invoke 인터페이스, 핸들 생명주기 |
| **Services** | C# | C API 래핑, TLE HTTP 다운로드 |
| **Models** | C# | 순수 데이터 (Satellite, GroundStation, PassEvent) |
| **ViewModels** | C# | UI 상태, Command, 실시간 타이머 |
| **Views** | XAML | 선언적 UI, 데이터 바인딩 |

## 데이터 흐름

### 실시간 위치 업데이트 (1초 주기)

```
DispatcherTimer.Tick
  └── foreach satellite
        └── OrbitEngineService.GetPosition(handle, now)
              └── orbit_get_position() [C++]
                    └── SGP4.propagateUnix(unix_ts)
                          ├── tsince = (now - epoch) / 60
                          ├── SGP4 전파 → ECI 벡터
                          └── eciToGeodetic() → lat/lon/alt
              ← GeoPositionC
        └── SatelliteViewModel.Latitude/Longitude/Altitude 갱신
              └── OnPropertyChanged → XAML 자동 갱신
```

### 패스 예측 흐름

```
"패스 예측" 버튼 클릭
  └── PredictPassesCommand.Execute()
        └── OrbitEngineService.PredictPasses(handle, gs, start, 48h)
              └── orbit_predict_passes() [C++]
                    └── PassPredictor.predict()
                          ├── 15초 간격 앙각 스캔
                          └── 이분법 25회 → AOS/LOS 정밀화
              ← PassEventC[]
        └── ObservableCollection<PassEvent> 갱신
              └── DataGrid 자동 갱신
```
