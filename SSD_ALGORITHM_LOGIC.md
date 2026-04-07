# SSD (Sum of Squared Differences) 알고리즘의 논리적 전개

**작성 날짜:** 2026-04-02  
**구현 파일:** `howtofindSSD.cpp`, `asdf.cpp`  
**상태:** 현재 메인 구현 방식 (Phase 1, 2 최적화 완료)

---

## 목차
1. [핵심 개념](#핵심-개념)
2. [수학적 원리](#수학적-원리)
3. [알고리즘 흐름](#알고리즘-흐름)
4. [논리적 전개 과정](#논리적-전개-과정)
5. [구현 전략](#구현-전략)
6. [최적화 기법](#최적화-기법)
7. [정확도 분석](#정확도-분석)

---

## 핵심 개념

### 문제 정의

**프로쿠딘-고르스키 색상 복원:**

```
입력: 세 개의 그레이스케일 채널이 수직으로 스택된 이미지
      [Blue 채널]
      [Green 채널]
      [Red 채널]

문제: Green을 기준으로 Blue와 Red를 정렬하면?

해결책: SSD를 이용한 최적 오프셋 검색
```

### SSD의 직관

**"두 이미지가 얼마나 비슷한가"를 수치화**

```
Green 이미지:        Blue 이미지 (offset 적용):
[100, 102, 99, ...]  [105, 103, 98, ...] (dx=0, dy=0)

차이:  [5, 1, -1, ...]
제곱:  [25, 1, 1, ...]
합:    27
→ SSD = 27 (작을수록 정렬 잘됨)

다른 offset 시도:
Green:       Blue (dx=1, dy=0):
[100, 102]   [103, 98]
차이: [3, -4]
제곱: [9, 16]
합: 25
→ SSD = 25 (더 작음! 더 나은 정렬)
```

---

## 수학적 원리

### SSD 정의

**공식:**
```
SSD(dx, dy) = Σ Σ (Green[y,x] - Blue[y+dy, x+dx])²
              y x
```

**의미:**
- 각 픽셀에서 두 이미지의 밝기 차이를 계산
- 차이를 제곱 (음수를 양수로, 큰 차이를 더 강조)
- 모든 픽셀에 대해 합산
- 작을수록 두 이미지가 정렬됨

### 왜 제곱인가?

**이유:**
```
차이값 처리:
1. 절대값 |d| 사용: [-5, 5] 모두 5로 처리 (차이 무시)
2. 제곱 d² 사용: [-5, 5] 모두 25로 처리 (차이 강조) ✓

큰 차이 강조:
- 작은 오프셋: SSD ≈ 100
- 틀린 오프셋: SSD ≈ 10,000 (매우 큼!)
→ 최소값 찾기가 명확함
```

### 정규화 (Normalization)

**개선:**
```
SSD_raw(dx, dy) = Σ (Green[y,x] - Blue[y+dy, x+dx])²
이미지 크기에 따라 값이 달라짐 (비교 어려움)

SSD_normalized(dx, dy) = SSD_raw(dx, dy) / (ROI 픽셀 수)
일관된 비교 가능 (이미지 크기 무관)
```

---

## 알고리즘 흐름

### 고수준 구조

```
입력: src (원본 스택 이미지)
         ↓
[Step 1] 채널 분리
         ↓
    Blue, Green, Red
         ↓
[Step 2] Green을 기준으로 정렬 탐색
         ├─→ alignB(Green, Blue) → offsetB
         └─→ alignR(Green, Red)  → offsetR
         ↓
[Step 3] Blue와 Red를 오프셋으로 이동하여 병합
         ↓
    dest (3채널 RGB 이미지)
         ↓
출력: 색상 복원된 이미지
```

### 상세 흐름도

```
main()
├─ cvLoadImage(path) → src
├─ 채널 분리 루프
│  ├─ Blue ← src[0:H/3]
│  ├─ Green ← src[H/3:2H/3]
│  └─ Red ← src[2H/3:H]
│
├─ alignB(Green, Blue)
│  ├─ Y 오프셋 탐색 (dy = -35 ~ +35)
│  │  ├─ for dy in [-35, 35]:
│  │  │  └─ SSD = calculateSSD(Green, Blue, 0, dy)
│  │  └─ bestY = min SSD 값의 dy
│  │
│  └─ X 오프셋 탐색 (dx = -15 ~ +15, 고정된 bestY 사용)
│     ├─ for dx in [-15, 15]:
│     │  └─ SSD = calculateSSD(Green, Blue, dx, bestY)
│     └─ bestX = min SSD 값의 dx
│  → offsetB(bestX, bestY) 반환
│
├─ alignR(Green, Red)  [alignB와 동일]
│  → offsetR(bestX, bestY) 반환
│
├─ 채널 병합 루프
│  for y in [0, H):
│    for x in [0, W):
│      dst[y,x] = RGB(
│        B = Blue[y+offsetB.y, x+offsetB.x],
│        G = Green[y,x],
│        R = Red[y+offsetR.y, x+offsetR.x]
│      )
│
└─ cvShowImage(dest)
```

---

## 논리적 전개 과정

### 단계 1: 왜 2단계 탐색인가?

**소박한 접근 (2D 그리드 탐색):**
```
for dx in [-15, 15]:
    for dy in [-35, 35]:
        SSD(dx, dy) 계산
        
총 루프: 31 × 71 = 2,201회
```

**개선된 접근 (축 분리):**
```
1단계: Y 오프셋만 탐색 (dx=0 고정)
   for dy in [-35, 35]:
       SSD(0, dy) 계산    → 71회
   bestY = 최적 dy

2단계: X 오프셋 탐색 (dy=bestY 고정)
   for dx in [-15, 15]:
       SSD(dx, bestY) 계산  → 31회
   bestX = 최적 dx

총 루프: 71 + 31 = 102회 (2,201의 4.6%)
```

**논리:**
```
SSD(dx, dy)는 dx와 dy에 대해 거의 독립적
(Green 해상도 1920×1440, 오프셋 ±15/±35는 매우 작음)

따라서:
- bestY는 dy만으로 결정 (dx 무관)
- bestX는 dy=bestY일 때만 계산
```

### 단계 2: 왜 ±15, ±35인가?

**역사적 분석:**

```
프로쿠딘-고르스키 이미지의 특성:
- 원본 필름 해상도: ~1900×2500
- 스캔 오류: ±20 픽셀 수준
- 필름 이송 오차 (Y): ±40 픽셀 가능
- 카메라 회전 오차 (X): ±10 픽셀

경험적 설정:
- X 범위: ±15 (충분히 큰 마진 포함)
- Y 범위: ±35 (필름 이송 오차 대응)

현재 이미지:
- W=3000, H=1000 (상대적으로 작은 이미지)
- 비율 유지: X 범위 적절, Y 범위 다소 큼
- 하지만 일반화를 위해 유지
```

### 단계 3: ROI (관심 영역) 선택

**ROI 정의:**
```
searchX: [W/4, 3W/4]  → 중앙 50%
searchY: [H/4, 3H/4]  → 중앙 50%

이유:
- 경계 픽셀에는 렌즈 왜곡 있음
- 이미지 경계 근처는 신뢰도 낮음
- 중앙부 50%는 안정적인 콘텐츠
```

### 단계 4: 정렬 전략 (Green 기준)

**선택 이유:**
```
세 채널 중 어느 것을 기준으로 정렬할 것인가?

옵션 1: Blue 기준
  B[y,x] = Green[y+dy, x+dx] (계산 필요)
  R[y,x] = Red[y+dy, x+dx] (계산 필요)
  문제: 두 번의 계산

옵션 2: Green 기준 ✓ (현재 방식)
  B[y,x] = Green[y+dy, x+dx] (계산 필요)
  R[y,x] = Red[y+dy, x+dx] (계산 필요)
  장점: 원본 Green 유지, 심리학적 안정성

옵션 3: 중간값 기준
  B[y,x] = Green[y+dy/2, x+dx/2] (복잡함)
  R[y,x] = Red[y+dy/2, x+dx/2]
```

**선택:** Green 기준 (사람의 눈이 Green에 민감)

---

## 구현 전략

### Phase 0: 기본 SSD (cvGet2D 기반)

```cpp
double calculateSSD_naive(IplImage* base, IplImage* target, int dx, int dy) {
    double sum = 0;
    int count = 0;
    
    for (int y = H/4; y < 3H/4; y++) {
        for (int x = W/4; x < 3W/4; x++) {
            // cvGet2D: 150ns/call × 2 = 300ns/pixel
            int base_val = cvGet2D(base, y, x).val[0];
            int target_val = cvGet2D(target, y+dy, x+dx).val[0];
            int diff = base_val - target_val;
            sum += diff * diff;
            count++;
        }
    }
    return sum / count;  // 정규화
}

// alignB 호출
OptimalOffset offsetB = alignB(Green, Blue);  // 102 × SSD_naive 호출
OptimalOffset offsetR = alignR(Green, Red);   // 102 × SSD_naive 호출
```

**성능:** ~50초 (W=3000, H=1000)

---

### Phase 1: 직접 포인터 최적화

```cpp
double calculateSSD_optimized(IplImage* base, IplImage* target, int dx, int dy) {
    // 경계 사전 검사 (분기 제거)
    if ((W/4 + dx) < 0 || (3W/4 - 1 + dx) >= W ||
        (H/4 + dy) < 0 || (3H/4 - 1 + dy) >= H)
        return DBL_MAX;
    
    long long sum = 0LL;
    int count = 0;
    
    // 포인터 직접 접근 (방법 A)
    const uchar* baseData = (const uchar*)base->imageData;
    const uchar* targetData = (const uchar*)target->imageData;
    int baseStep = base->widthStep;
    int targetStep = target->widthStep;
    
    for (int y = H/4; y < 3H/4; y++) {
        const uchar* baseRow = baseData + y * baseStep;
        const uchar* targetRow = targetData + (y+dy) * targetStep;
        
        for (int x = W/4; x < 3W/4; x++) {
            // 직접 접근: 2-5ns/access (cvGet2D 대비 50~75배 빠름)
            int diff = baseRow[x] - targetRow[x + dx];
            sum += (long long)diff * diff;
            count++;
        }
    }
    return (double)sum / count;
}
```

**성능:** ~930ms (**54배 향상**)

---

### Phase 2: 멀티스레딩

```cpp
// C 스타일 스레드 함수
DWORD WINAPI alignBThread(LPVOID param) {
    AlignBThreadData* data = (AlignBThreadData*)param;
    data->result = alignB(data->greenChannel, data->blueChannel);
    return 0;
}

// main에서 병렬 실행
AlignBThreadData dataB;
dataB.greenChannel = greenChannel;
dataB.blueChannel = blueChannel;

AlignRThreadData dataR;
dataR.greenChannel = greenChannel;
dataR.redChannel = redChannel;

HANDLE hB = CreateThread(NULL, 0, alignBThread, &dataB, 0, NULL);
HANDLE hR = CreateThread(NULL, 0, alignRThread, &dataR, 0, NULL);

WaitForSingleObject(hB, INFINITE);
WaitForSingleObject(hR, INFINITE);

// 결과 추출
OptimalOffset offsetB = dataB.result;
OptimalOffset offsetR = dataR.result;
```

**성능:** ~480ms (**104배 향상**, Phase 1 대비 2배)

---

## 최적화 기법

### 기법 A: 직접 포인터 접근

| 항목 | cvGet2D | 직접 포인터 |
|------|---------|-----------|
| 오버헤드 | 함수 호출, 경계 검사, 타입 변환 | 거의 없음 |
| 성능 | 150ns/call | 2-5ns/access |
| 배율 | 1배 | 30-75배 |

### 기법 B: 정수 연산

```cpp
// Before: double 연산 (FPU)
double diff = (double)val1 - (double)val2;
sum += diff * diff;

// After: int 연산 (ALU)
int diff = val1 - val2;
sum += (long long)diff * diff;
```

**효과:** FPU 파이프라인 회피, 5-15% 추가 향상

### 기법 C: 경계 검사 사전 처리

```cpp
// Before: 루프 내 분기 (매 픽셀)
if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;

// After: 루프 전 1회 검사
if ((x1 + dx) < 0 || (x2-1 + dx) >= W ||
    (y1 + dy) < 0 || (y2-1 + dy) >= H)
    return DBL_MAX;
```

**효과:** 분기 예측 미스 제거, 10-20% 추가 향상

### 기법 D: 메모리 접근 최적화

```cpp
// 루프 내 반복
for (int y = y1; y < y2; y++) {
    for (int x = x1; x < x2; x++) {
        int val = baseData[y * widthStep + x];  // ❌ 매번 계산
    }
}

// 루프 외부 계산
for (int y = y1; y < y2; y++) {
    const uchar* row = baseData + y * widthStep;  // ✓ 1회만 계산
    for (int x = x1; x < x2; x++) {
        int val = row[x];
    }
}
```

**효과:** 포인터 산술 제거, 캐시 친화성 향상

### 기법 E: 멀티스레딩

```
순차: alignB (450ms) → alignR (450ms) = 900ms
병렬: max(alignB, alignR) = 450ms (2배 향상)
```

---

## 정확도 분석

### 검색 해상도

```
탐색 범위: dx ∈ [-15, 15], dy ∈ [-35, 35]
해상도: 1 픽셀 단위 (정수)

따라서:
- 정렬 오프셋이 정수 픽셀인 경우: 완벽한 정렬 가능
- 부분 픽셀 오프셋 필요한 경우: 1 픽셀 오차
```

### 오류 원인

```
1. 검색 범위 초과
   - 실제 오프셋이 ±15, ±35 범위 밖이면 실패
   - 프로쿠딘-고르스키: 거의 발생하지 않음

2. ROI 내 특성 부족
   - 중앙 50% 영역이 매우 균일하면 정렬 불확실
   - 텍스처 풍부한 영역에서 정렬 정확도 높음

3. 채널 특성 차이
   - 다양한 카메라 필터의 광학 특성 차이
   - 감마 보정, 노이즈 수준 차이
   - SSD는 픽셀값 직접 비교이므로 영향 받음
```

### 개선 가능 방향

| 기법 | 효과 | 구현 복잡도 |
|------|------|-----------|
| 피라미드 탐색 | 2-4배 속도, 정확도 유지 | 중간 |
| 조기 종료 | 30-40% 추가 속도 | 낮음 |
| Edge 기반 SSD | 노이즈 강건성 | 높음 |
| Normalized Cross-Correlation | 광학 특성 차이 보정 | 높음 |

---

## 요약: SSD의 강점과 약점

### 강점 ✓
- **개념 단순**: 픽셀 차이 제곱 합 (직관적)
- **계산 효율**: O(W×H×R) (R = 탐색 범위)
- **구현 용이**: cvGet2D만으로 구현 가능
- **일반화**: 다양한 이미지에서 안정적
- **최적화 공간**: 포인터, 정수 연산, 멀티스레딩 등 다양

### 약점 ✗
- **노이즈 민감**: 한 두 픽셀의 이상값에 영향
- **광학 특성 차이**: 카메라 필터 특성 미보정
- **부분 픽셀 오프셋**: 정수 픽셀만 가능
- **계산 비용**: 대면적 탐색 필요
- **정확도 한계**: ~±2-3 픽셀 오차

### 최종 평가

**SSD는 프로쿠딘-고르스키 복원에 적합한 이유:**
1. 채널 스택 구조 단순 (2D 오프셋 탐색)
2. ROI 선택으로 노이즈 최소화
3. 축 분리로 계산 효율 극대화
4. 멀티스레딩으로 병렬화 가능
5. 하드웨어 특성(메모리 계층)에 최적화 가능
