# SGP4 알고리즘 학습 노트

> Simplified General Perturbations 4 — 위성 궤도 예측의 국제 표준 알고리즘

## 1. 왜 SGP4인가?

위성은 완벽한 케플러 타원 궤도를 따르지 않습니다. 실제로는 여러 **섭동(perturbation)** 이 존재합니다.

| 섭동 원인 | 효과 | SGP4 모델링 |
|----------|------|------------|
| 지구 비구형성 (J₂, J₄) | RAAN·근지점 세차 | O (주요 항) |
| 대기 항력 | 궤도 감쇠 | O (BSTAR 계수) |
| 달/태양 중력 | 장주기 섭동 | X (SDP4에서 처리) |
| 태양풍/복사압 | 미세 | X |

| 모델 | 대상 궤도 | 고도 |
|------|---------|------|
| **SGP4** | LEO (저궤도) | < ~2000 km |
| SDP4 | MEO/GEO/HEO | > ~2000 km |

이 프로젝트는 ISS, 기상위성 등 LEO 대상인 SGP4만 구현합니다.

---

## 2. 입력: TLE (Two-Line Element Set)

NORAD가 정의한 위성 궤도 기술 표준 형식입니다.

```
ISS (ZARYA)
1 25544U 98067A   24001.50000000  .00006089  00000+0  11294-3 0  9993
2 25544  51.6416 290.5000 0001234  80.0000 280.0000 15.49000000440000
```

### Line 1 파싱

```
1 25544U 98067A   24001.50000000  .00006089  00000+0  11294-3 0  9993
│ │     │ │       │               │           │         │
│ │     │ │       에폭(YYDDD.DDD)  ṅ/2         ṅ̈/6      B* (BSTAR)
│ │     │ 국제식별자
│ 위성번호
라인번호
```

- **에폭** `24001.50`: 2024년 1.5일 = 2024-01-01 12:00:00 UTC
- **BSTAR** `11294-3` → `0.11294 × 10⁻³` (가정소수점 형식)

### Line 2 파싱

```
2 25544  51.6416 290.5000 0001234  80.0000 280.0000 15.49000000440000
         i₀      Ω₀       e₀       ω₀       M₀       n₀         혁명번호
```

| 기호 | 이름 | 단위 | 의미 |
|------|------|------|------|
| i₀ | 궤도경사각 | ° | 궤도면과 적도면의 각도 |
| Ω₀ | 승교점 적경 (RAAN) | ° | 궤도의 적도 교차점 방향 |
| e₀ | 이심률 | — | 0(원) ~ 1(포물선) |
| ω₀ | 근지점 편각 | ° | 가장 낮은 점의 방향 |
| M₀ | 평균 근점이각 | ° | 에폭 시각의 위성 위치 |
| n₀ | 평균 운동 | rev/day | 하루 공전 횟수 |

---

## 3. 케플러 궤도 기초

### 궤도 요소 → 위치 계산 흐름

```
평균 근점이각 M (시간에 따라 선형 증가)
    │
    ▼  케플러 방정식: M = E - e·sin(E)
이심 근점이각 E  (Newton-Raphson 반복 풀이)
    │
    ▼  tan(ν/2) = √((1+e)/(1-e)) · tan(E/2)
진 근점이각 ν
    │
    ▼  r = a(1 - e·cos(E))
궤도 반경 r + 위도 편각 u = ν + ω
    │
    ▼  i, Ω 회전 적용
ECI 좌표 (x, y, z) [km]
```

### 케플러 방정식 — Newton-Raphson 풀이

M = E - e·sin(E) 는 E에 대한 초월방정식으로 수치 반복법으로 풉니다.

```cpp
double solveKepler(double M, double e) {
    double E = M;           // 초기값: E₀ = M
    for (int i = 0; i < 50; ++i) {
        double dE = (M - E + e * std::sin(E)) / (1.0 - e * std::cos(E));
        E += dE;
        if (std::abs(dE) < 1e-12) break;  // 수렴 판정
    }
    return E;
}
```

보통 5~10회 반복으로 수렴합니다 (이심률 e < 0.9 기준).

---

## 4. SGP4의 섭동 모델

### 4.1 지구 비구형성 (J₂, J₄ 항)

지구는 적도가 약 21 km 불룩한 회전 타원체입니다. 중력 포텐셜을 구면조화함수로 전개합니다.

```
U = (μ/r)[1 - J₂(Rₑ/r)²P₂(sin φ) - J₄(Rₑ/r)⁴P₄(sin φ) - ...]
```

- **J₂ = 1.08263 × 10⁻³** — 가장 크고 중요한 항
- **J₄ = -1.65597 × 10⁻⁶** — 고차 보정 항

J₂로 인한 세차:
- **RAAN 세차 (Ω̇)** : 궤도면이 천천히 회전 → 태양동기궤도(SSO) 원리
- **근지점 세차 (ω̇)** : 근지점이 궤도면 내에서 회전

```cpp
// 세차율 계산 (rad/min)
double raandot = -k2 * cos(i0) * n0 / (p*p) / betao2;
double argpdot = 0.75 * k2 * (5*cos²i - 1) * n0 / (p*p) / betao2;
```

### 4.2 대기 항력 (BSTAR 항)

저궤도 위성은 극히 희박한 대기와 충돌하며 에너지를 잃습니다.

```
항력 = ½ · ρ · Cd · A/m · v²    (ρ: 대기밀도, Cd: 항력계수, A/m: 면적/질량비)
```

TLE의 BSTAR = ½ · ρ₀ · Cd · A/m (단순화된 표현)

SGP4에서 반장축 감소:
```
a(t) = a₀ · (1 - C₁·t - D₂·t² - D₃·t³ - D₄·t⁴)²
```

### 4.3 SGP4 전파 흐름

```
입력: t (에폭 이후 경과 분)
  │
  ├── 세속 항 (secular terms)
  │    M(t) = M₀ + Ṁ·t              평균 근점이각 전파
  │    ω(t) = ω₀ + ω̇·t              근지점 편각 세차
  │    Ω(t) = Ω₀ + Ω̇·t              RAAN 세차
  │    a(t) = a₀·(1 - C₁·t)²        반장축 감소 (항력)
  │    e(t) = e₀ - B*·C₄·t          이심률 변화 (항력)
  │
  ├── 케플러 방정식 풀기: M(t) → E → ν
  │
  ├── 궤도면 위치: (r, u=ν+ω)
  │
  ├── ECI 좌표 계산 (i, Ω 회전)
  │    x = r·(cos Ω·cos u - sin Ω·sin u·cos i)
  │    y = r·(sin Ω·cos u + cos Ω·sin u·cos i)
  │    z = r·(sin i·sin u)
  │
  └── 출력: pos[3] (km), vel[3] (km/s)
```

---

## 5. 좌표 변환 체계

### 5.1 ECI (Earth-Centered Inertial)

- X축: 춘분점 방향 (항성에 대해 고정)
- Z축: 북극 방향
- SGP4 출력 좌표계

### 5.2 ECEF (Earth-Centered, Earth-Fixed)

지구와 함께 자전하는 좌표계.

```
[x_ecef]   [ cos θ  sin θ  0 ] [x_eci]
[y_ecef] = [-sin θ  cos θ  0 ] [y_eci]
[z_ecef]   [  0       0    1 ] [z_eci]

θ = GMST (그리니치 평균 항성시, 라디안)
```

GMST 계산:
```cpp
double computeGMST(double unix_timestamp) {
    double jd = unix_timestamp / 86400.0 + 2440587.5;
    double T  = (jd - 2451545.0) / 36525.0;  // Julian centuries
    double theta = 67310.54841
                 + (876600.0*3600.0 + 8640184.812866) * T
                 + 0.093104 * T*T - 6.2e-6 * T*T*T;
    return fmod(theta, 86400.0) * 2*PI / 86400.0;  // → 라디안
}
```

### 5.3 Geodetic (WGS84)

ECEF → 위경도/고도 변환 (Bowring 반복법):

```cpp
double lon = atan2(y_ecef, x_ecef);           // 경도 (직접 계산)
double p   = sqrt(x*x + y*y);                 // 적도면 거리

// 위도 반복 수렴
double lat = atan2(z, p * (1 - e²));
for (int i = 0; i < 10; ++i) {
    double N = a / sqrt(1 - e²·sin²(lat));    // 곡률반경
    lat = atan2(z + e²·N·sin(lat), p);
}

double alt = p / cos(lat) - N;                // 고도
```

### 5.4 SEZ (패스 예측용)

지상국 기준 위성 방향 계산:

```
SEZ = South-East-Zenith 좌표계
  S: 남쪽, E: 동쪽, Z: 천정 방향

범위 벡터 (위성ECEF - 지상국ECEF) → SEZ 변환
elevation = arcsin(rZ / |r|)
azimuth   = atan2(-rE, rS)   (북향 기준 시계방향)
```

---

## 6. 패스 예측 (AOS/LOS)

### 탐색 알고리즘

```
1단계: Coarse scan (15초 간격)
  t = start → end, Δt = 15s
  앙각(t) 계산
  앙각이 min_elev° 경계를 교차하면 → 구간 발견

2단계: 이분법 정밀화 (25회 반복)
  t0, t1 = 교차 구간 양 끝
  for 25회:
      tm = (t0+t1)/2
      if elev(tm) >= min_elev: t0 = tm
      else:                     t1 = tm
  → AOS/LOS 시각 오차 < ~0.5초
```

```cpp
// 이분법 AOS 탐색
double t0 = t - step, t1 = t;
for (int i = 0; i < 25; ++i) {
    double tm = 0.5*(t0 + t1);
    if (elevation(tm) >= min_elevation) t1 = tm;
    else t0 = tm;
}
double aos = 0.5*(t0 + t1);
```

---

## 7. 참고 자료

| 자료 | 링크/출처 |
|------|----------|
| Vallado, "Fundamentals of Astrodynamics" | 주요 참조 구현 |
| Hoots & Roehrich, SPACETRACK Report #3 (1980) | SGP4 원 논문 |
| CelesTrak | TLE 데이터 무료 제공 |
| WGS84 표준 | 지구 타원체 파라미터 |
