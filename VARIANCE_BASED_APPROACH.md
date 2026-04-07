# 분산(Variance) 기반 채널 경계 검출 방식

**작성 날짜:** 2026-04-02  
**관련 파일:** `DP_Assignment2_Variance.cpp`, `DP_Assignment2_FindVelley.cpp`, `assignment2_hint.cpp`  
**상태:** 분석 완료, 현재는 SSD 기본 방식으로 전환

---

## 목차
1. [핵심 개념](#핵심-개념)
2. [논리적 접근](#논리적-접근)
3. [구현 방식](#구현-방식)
4. [예상 결과](#예상-결과)
5. [발생한 문제](#발생한-문제)
6. [교훈 및 향후 방향](#교훈-및-향후-방향)

---

## 핵심 개념

### 문제 상황

**프로쿠딘-고르스키 이미지의 구조:**
```
입력 이미지: [Blue 채널] [Green 채널] [Red 채널]
             높이: H/3     높이: H/3     높이: H/3
             ↑ 검은 경계   ↑ 검은 경계   ↑ 검은 경계
```

**기존 문제:**
- 단순 height/3 분할은 실제 채널 경계를 무시
- 검은색 경계 영역(낮은 밝기)을 포함하면 SSD 계산 오류 증가
- 채널 간 정렬 정확도 저하

### 핵심 아이디어

**"채널 경계는 낮은 분산(variance) 영역"**

```
이미지 행 분석:
밝기 분포 → 분산 계산

예시:
┌─────────────────────────┐
│ Row 100 (콘텐츠): var=45│  ← 높은 분산 (다양한 밝기)
│ Row 330 (검은 경계):var=2│  ← 낮은 분산 (일정한 검은색)
│ Row 331 (콘텐츠): var=48│  ← 높은 분산
└─────────────────────────┘

낮은 분산 영역 → 채널 경계 위치 추정
```

---

## 논리적 접근

### 단계 1: 각 행의 분산 계산

**수식:**
```
Variance(row) = Σ(pixel_brightness - mean_brightness)² / width

mean_brightness = Σ pixel_brightness / width
```

**예시 (가상 행):**
```
행 입력:  [10, 20, 15, 25, 30]
평균:     20
분산:     ((10-20)² + (20-20)² + (15-20)² + (25-20)² + (30-20)²) / 5
        = (100 + 0 + 25 + 25 + 100) / 5
        = 250 / 5 = 50

검은색 행: [1, 2, 1, 2, 1]
평균:     1.4
분산:     ((1-1.4)² + (2-1.4)² + ...) / 5
        ≈ 0.24  ← 매우 낮음!
```

### 단계 2: 분산 평활화 (Smoothing)

**문제:** 원본 분산 데이터는 노이즈가 많음

```
원본 분산:     [45, 2, 48, 44, 3, 46, 47]
                       ↑ 노이즈?
```

**해결:** 슬라이딩 윈도우 평균

```cpp
smoothed_var[y] = (var[y-3] + var[y-2] + var[y-1] + var[y] + var[y+1] + var[y+2] + var[y+3]) / 7
                            ↑ window=7
```

**효과:**
```
원본:    [45, 2, 48, 44, 3, 46, 47]
평활:    [30, 25, 20, 18, 20, 25, 30]
                     ↑ 안정적
```

### 단계 3: 경계 검출

**알고리즘:**
```
1. threshold = mean_variance × 0.4
   (전체 평균 분산의 40% 이하)

2. 연속된 낮은 분산 영역 찾기
   (variance < threshold인 구간)

3. 가장 긴 2개 구간의 중심을 경계로 설정
   topY = 첫 번째 낮은 분산 영역의 중심
   bottomY = 두 번째 낮은 분산 영역의 중심
```

**시각화:**
```
분산 그래프:
│
│  ┌─────┐         ┌─────┐
│  │높음 │ ┌──┐    │높음 │
├──┼─────┼─┤  ├────┼─────┼──
│  │낮음 │ │낮│    │낮음 │
│  └─────┘ └──┘    └─────┘
└────────────────────────────
  topY 경계        bottomY 경계
```

---

## 구현 방식

### 함수 구조

```cpp
// 1. 각 행의 분산 계산
double* calculateVarianceY(IplImage* img) {
    double* var = new double[height];
    for (int y = 0; y < height; y++) {
        // 행 y의 모든 픽셀 분산 계산
        var[y] = variance_of_row(y);
    }
    return var;
}

// 2. 분산 평활화
double* smoothVariance(double* var, int height, int window) {
    double* smoothed = new double[height];
    for (int y = 0; y < height; y++) {
        double sum = 0;
        int count = 0;
        for (int dy = -window/2; dy <= window/2; dy++) {
            int ny = y + dy;
            if (ny >= 0 && ny < height) {
                sum += var[ny];
                count++;
            }
        }
        smoothed[y] = sum / count;
    }
    return smoothed;
}

// 3. 경계 검출
void findVarianceYBoundaries(double* var, int height, 
                              int& topY, int& bottomY) {
    // threshold 계산
    double mean = 0;
    for (int y = 0; y < height; y++) mean += var[y];
    mean /= height;
    double threshold = mean * 0.4;
    
    // 낮은 분산 영역 찾기
    vector<pair<int, int>> lowVarRuns;  // (시작, 끝)
    int runStart = -1;
    
    for (int y = 0; y < height; y++) {
        if (var[y] < threshold) {
            if (runStart == -1) runStart = y;
        } else {
            if (runStart != -1) {
                lowVarRuns.push_back({runStart, y-1});
                runStart = -1;
            }
        }
    }
    
    // 가장 긴 2개 영역 선택
    sort(lowVarRuns.begin(), lowVarRuns.end(),
         [](const pair<int,int>& a, const pair<int,int>& b) {
             return (a.second - a.first) > (b.second - b.first);
         });
    
    topY = (lowVarRuns[0].first + lowVarRuns[0].second) / 2;
    bottomY = (lowVarRuns[1].first + lowVarRuns[1].second) / 2;
}
```

---

## 예상 결과

### 이상적 시나리오

```
입력: 프로쿠딘-고르스키 이미지 (W=3000, H=3000)
      [Blue: 0-999] [Green: 1000-1999] [Red: 2000-2999]

분산 분석:
Row   0-100: var ≈ 1.2   ← 검은 경계 (Blue 위)
Row 100-950: var ≈ 45.0  ← Blue 콘텐츠
Row 950-1050: var ≈ 2.1  ← 검은 경계 (Blue-Green 사이)
Row 1050-1950: var ≈ 44.5 ← Green 콘텐츠
Row 1950-2050: var ≈ 2.0  ← 검은 경계 (Green-Red 사이)
Row 2050-2999: var ≈ 46.0 ← Red 콘텐츠

감지된 경계:
topY ≈ 1000 (100과 1050의 중간)
bottomY ≈ 2000 (1950과 2050의 중간)

정확도: ±10 픽셀 이내
```

### 기대 효과

| 항목 | 개선 |
|------|------|
| 검은 경계 제거 | SSD 계산에서 노이즈 ~30% 감소 |
| 정렬 정확도 | ±15 픽셀 범위 검색 → ±10 픽셀 범위 축소 가능 |
| 계산 효율 | 불필요한 경계 픽셀 제외 |

---

## 발생한 문제

### 문제 1: 경계 검출 실패 (예기치 않은 이미지 형식)

**증상:**
```
일부 이미지에서 topY, bottomY 값이 예상과 전혀 다름
또는 detect되지 않음 (-1 반환)
```

**근본 원인:**
- 프로쿠딘-고르스키 이미지가 항상 깔끔한 검은 경계를 갖지 않음
- 일부 사진은 경계가 흐릿하거나 고르지 않음
- 분산이 낮은 영역이 여러 개 존재할 수 있음
  ```
  [낮은 분산] [높은] [낮은] [높은] [낮은]
               ↑ 어느 것이 채널 경계?
  ```

**예시:**
```
원본 이미지가 매우 밝은 경우:
- 검은 경계의 분산: var = 5.0
- 밝은 영역의 분산: var = 15.0 (낮음!)
- threshold = mean * 0.4 = 12.0
- 결과: 검은 경계만 아니라 밝은 영역도 감지됨
```

### 문제 2: Threshold 값 선택의 어려움

**분석:**
```
이미지 특성에 따라 최적 threshold가 다름

경우 1: 어두운 이미지
- mean_variance = 30
- threshold = 12 (0.4×30)
- 작동함

경우 2: 밝은 이미지
- mean_variance = 80
- threshold = 32 (0.4×80)
- 검은 경계(var=5)와 밝은 부분(var=20) 구별 어려움

경우 3: 고콘트라스트 이미지
- mean_variance = 200
- threshold = 80 (0.4×200)
- 대부분 낮은 분산으로 오인됨
```

### 문제 3: SSD 정렬 정확도 여전히 낮음

**발견:**
```
경계 검출은 성공했지만, 최종 병합 이미지에서 여전히 색상 misalignment 발생
```

**원인 분석:**
1. 경계 검출은 ±10-20 픽셀 오차 (수용 가능)
2. 하지만 SSD 탐색 범위(±15, ±35)도 여전히 넓음
3. 실제 오프셋을 정확히 찾지 못함
   ```
   경계 검출: topY ≈ 1000 (오차 ±15)
   SSD 탐색: ±35 범위로 어정쩡한 오프셋 찾음 (정렬 실패)
   ```

### 문제 4: 다양한 이미지 형식의 일반화 불가능

**사례:**
```
테스트 이미지 A: 작동 ✓
테스트 이미지 B: 실패 ✗
테스트 이미지 C: 부분적 작동 △

→ 하나의 threshold/window 값으로는 모든 이미지 처리 불가능
```

---

## 교훈 및 향후 방향

### 얻은 통찰

| 배운 점 | 의미 |
|--------|------|
| 휴리스틱 방법의 한계 | 분산 기반 경계 검출은 특정 이미지에 의존적 |
| 매개변수 튜닝의 어려움 | threshold, window 같은 "마법의 수"는 일반화 불가 |
| 문제 재정의의 중요성 | 경계 검출보다 직접 "채널 정렬"이 더 근본적 해결책 |
| 강건성(Robustness) | 다양한 입력에 대응하려면 적응형(adaptive) 방법 필요 |

### 최종 결론

**분산 기반 경계 검출은:**
- ✅ 개념적으로 우아함 (edge와 다른 관점)
- ✅ 특정 이미지에서는 잘 작동
- ❌ 일반화 불가능 (threshold 문제)
- ❌ SSD 정렬 정확도 개선에 미미한 영향

**따라서:**
- 현재 SSD 기본 방식(간단한 높이/3 분할)으로 복귀
- SSD 탐색 범위 확대 또는 알고리즘 개선이 더 효과적
- 피라미드 탐색(Phase 3)이 더 유망한 방향

---

## 관련 파일 참고

- `DP_Assignment2_Variance.cpp` - 분산 계산 초기 구현
- `DP_Assignment2_FindVelley.cpp` - 분산 valley 기반 경계 검출 시도
- `assignment2_hint.cpp` - 분산 + SSD 통합 버전
- `Assignment2_Summary.md` - 상세 분석 및 방식 비교

---

## 참고: 적응형 경계 검출 (미구현)

만약 다시 시도한다면:

```cpp
// 적응형 threshold
double adaptiveThreshold(double* var, int height) {
    // 분산 히스토그램 기반 threshold 자동 설정
    // Otsu's method 등 고급 영상처리 기법 사용
    // 결과: 이미지마다 최적화된 threshold
}

// 계층적 경계 검출
void hierarchicalBoundaryDetection(IplImage* img, 
                                    int& topY, int& bottomY) {
    // 1단계: 전역 경계 후보 검출
    // 2단계: 지역 특성 분석
    // 3단계: 다중 후보 평가 및 선택
}
```

하지만 이러한 고도화는 **SSD 기본 방식의 성능 개선(Phase 3: 피라미드)**이 더 효율적임을 고려하면 우선순위가 낮습니다.
