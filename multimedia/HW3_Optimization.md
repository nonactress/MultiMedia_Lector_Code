# HW3 최적화 분석 — Mean Filter (Integral Image)

> **대상 파일:** `HW3.cpp`  
> **최적화 방향:** 새로운 OpenCV 함수 사용 없이, 순수 C 언어 레벨에서 메모리·시간 복잡도 개선

---

## 1. 현재 코드 구조 개요

```
myFastestMeanFilter(src, dst, k)
├── setTable()        — 3채널 2D 누적합(Integral Image) 테이블 구성
│   └── cvGet2D() 호출 W×H 회 (픽셀 하나씩 읽기)
└── getSmooth() 호출 W×H 회 — 테이블에서 O(1)로 평균 계산
    ├── cvGetSize() 매 픽셀마다 호출
    ├── k/2 나눗셈 매 픽셀마다 수행
    └── cvSet2D() 로 결과 쓰기
```

### 알고리즘 원리 (Integral Image / Prefix Sum)

```
테이블 구성: T[y][x] = Σ img[j][i]  (0 ≤ j ≤ y, 0 ≤ i ≤ x)

임의 직사각형 구간 합 (O(1) 계산):
Sum = T[y1][x1] - T[y0][x1] - T[y1][x0] + T[y0][x0]
```

알고리즘 자체는 이미 최적 — **O(W×H)** 시간복잡도.  
하지만 **상수 인수(constant factor)**가 매우 크다. 이를 줄이는 것이 핵심이다.

---

## 2. 현재 코드의 문제점

### 2-1. 메모리 할당: 단편화 & 캐시 미스

**현재 코드 (9~17행):**
```cpp
int** sumTableR = (int**)malloc(sizeof(int*) * src->height);   // 1번
for (int i = 0; i < src->height; i++)
    sumTableR[i] = (int*)malloc(sizeof(int) * src->width);     // H번
```

- `malloc` 호출 횟수: 채널당 **H+1번** → 3채널 합계 **3H+3번**
- 각 행이 힙(heap) 상 **불연속** 위치에 배치됨

**문제:**
1. **힙 단편화(Heap Fragmentation):** 수백 번의 소형 malloc은 힙 메타데이터 오버헤드를 키운다.
2. **캐시 미스(Cache Miss):** 행렬을 순차적으로 읽어도 각 행이 서로 다른 캐시 라인에 산재 → L1/L2 캐시 효율 저하.

---

### 2-2. 픽셀 접근: cvGet2D / cvSet2D 함수 오버헤드 ★ 가장 큰 병목

**현재 코드 (setTable, 78~110행):**
```cpp
CvScalar c = cvGet2D(img, v, u);   // ← 매 픽셀마다 호출
b[v][u] = c.val[0] + ...;
```

`cvGet2D`는 내부적으로:
1. 이미지 타입(깊이·채널 수) 확인
2. 경계 검사
3. 타입별 분기(switch)
4. `CvScalar` 구조체 생성 및 반환

→ **픽셀 하나를 읽기 위해 수십 개의 명령어** 실행.  
→ `setTable`에서만 **W×H번** 호출 = 512×512 이미지 기준 **262,144번**.  
→ `myFastestMeanFilter`의 `cvSet2D`도 동일하게 W×H번 호출.

---

### 2-3. 반복 계산: 매 픽셀마다 같은 값 재계산

**현재 코드 (getSmooth, 31~75행):**
```cpp
CvSize size = cvGetSize(img);  // ← W×H번 호출
int minDx = x - k/2;          // ← k/2 나눗셈 W×H번
int maxDx = x + k/2;          // ← 동일
int minDy = y - k/2;          // ← 동일
int maxDy = y + k / 2;        // ← 동일
```

- `cvGetSize`는 구조체 복사를 수반하는 함수 호출
- `k/2` 정수 나눗셈은 같은 `k`에 대해 항상 동일한 결과 → 루프 밖으로 꺼낼 수 있음

---

### 2-4. 함수 호출 오버헤드

`getSmooth`가 W×H번 호출됨 → 매번 스택 프레임 생성·소멸, 인자 복사(포인터 배열 3개 포함).

---

### 2-5. 경계 처리 논리의 잠재적 버그

**현재 코드 (43~62행):**
```cpp
if (minDx < 0)  { offsetX = -minDx; ... }
if (maxDx > W-1){ offsetX = maxDx - (W-1); ... }  // ← 위에서 설정한 offsetX 덮어씀
```

픽셀이 이미지 왼쪽(`minDx<0`)과 오른쪽(`maxDx>W-1`) **양쪽** 동시에 벗어나는 경우
(k가 이미지 폭보다 클 때) `offsetX`가 두 번 덮어써져 `area` 계산이 틀린다.

또한 현재 `area` 계산:
```cpp
int area = (k - offsetX) * (k - offsetY);
```
이것은 `offset`이 한쪽 방향에서만 발생한다고 가정한 근사값이다.
실제 유효 영역 넓이는 `(x1-x0) * (y1-y0)` 이어야 정확하다.

---

## 3. 최적화 방안

### [최적화 1] 연속 메모리 블록 할당

**변경 전:**
```cpp
int** sumTableR = (int**)malloc(sizeof(int*) * H);
for (int i = 0; i < H; i++)
    sumTableR[i] = (int*)malloc(sizeof(int) * W);
```

**변경 후:**
```cpp
int* tableR = (int*)malloc(sizeof(int) * W * H);
// 2D 접근: tableR[y * W + x]
```

| 항목 | 변경 전 | 변경 후 |
|------|---------|---------|
| malloc 호출 수 (3채널) | 3H + 3 | **3** |
| 메모리 연속성 | 행별로 불연속 | **연속** |
| 캐시 효율 | 낮음 | **높음** |
| free 호출 수 | 3H + 3 | **3** |

---

### [최적화 2] cvGet2D / cvSet2D → imageData 직접 포인터 접근 ★ 최우선

IplImage의 픽셀 메모리 레이아웃:
```
imageData[y * widthStep + x * nChannels + ch]
```
- `widthStep`: 한 행의 바이트 수 (패딩 포함)
- `nChannels`: 채널 수 (BGR 컬러라면 3)

**setTable 변경 예시:**
```cpp
// 변경 전
CvScalar c = cvGet2D(img, v, u);
bVal = c.val[0]; gVal = c.val[1]; rVal = c.val[2];

// 변경 후
uchar* ptr = (uchar*)(img->imageData + v * img->widthStep + u * img->nChannels);
int bVal = ptr[0];
int gVal = ptr[1];
int rVal = ptr[2];
```

**myFastestMeanFilter 출력 변경 예시:**
```cpp
// 변경 전
cvSet2D(dst, y, x, c);

// 변경 후
uchar* dstPtr = (uchar*)(dst->imageData + y * dst->widthStep + x * dst->nChannels);
dstPtr[0] = (uchar)meanB;
dstPtr[1] = (uchar)meanG;
dstPtr[2] = (uchar)meanR;
```

**효과:** `cvGet2D`/`cvSet2D`의 내부 분기·타입체크 완전 제거.  
→ 픽셀 하나당 수십 개 명령어 → **3개 메모리 접근**으로 단축.

---

### [최적화 3] 반복 계산 루프 밖으로 추출 (Loop-Invariant Code Motion)

```cpp
// myFastestMeanFilter 내부에서 한 번만 계산
int W      = src->width;
int H      = src->height;
int half   = k / 2;        // 정수 나눗셈 1회
int wStep  = src->widthStep;
int nCh    = src->nChannels;

// getSmooth 시그니처 변경: k 대신 half, W, H를 전달
// → 함수 내부에서 cvGetSize, k/2 제거
```

W×H번 반복되던 나눗셈과 구조체 복사가 **각 1회**로 줄어든다.

---

### [최적화 4] 함수 인라인화

**방법 A — MSVC 강제 인라인:**
```cpp
static __forceinline void getSmoothAndWrite(...)
{
    // getSmooth 로직 + cvSet2D 대체 로직 합치기
}
```

**방법 B — 완전 인라인화 (가장 효과적):**
`getSmooth` 함수를 제거하고 `myFastestMeanFilter`의 이중 루프 내부에 직접 작성.  
→ 함수 호출 스택 오버헤드 W×H번 완전 제거.  
→ 컴파일러가 레지스터 할당·루프 언롤 등 추가 최적화 수행 가능.

---

### [최적화 5] 경계 처리 클램핑으로 단순화 + 정확도 수정

```cpp
// 변경 전 (offset 이중 덮어쓰기 위험)
if (minDx < 0)    { offsetX = -minDx; minDx = 0; }
if (maxDx > W-1)  { offsetX = maxDx - (W-1); maxDx = W-1; }
int area = (k - offsetX) * (k - offsetY);

// 변경 후 (명확한 clamp + 정확한 area)
int x0 = (x - half < 0)   ? 0   : x - half;
int x1 = (x + half > W-1) ? W-1 : x + half;
int y0 = (y - half < 0)   ? 0   : y - half;
int y1 = (y + half > H-1) ? H-1 : y + half;
int area = (x1 - x0) * (y1 - y0);
if (area == 0) { /* skip */ }
```

- `offsetX`/`offsetY` 변수 삭제 → 코드 단순화
- 양쪽 경계 동시 초과 시에도 정확한 `area` 계산

---

### [최적화 6] setTable 루프 내 행 포인터 캐싱

**변경 전 (행 포인터를 매 열마다 역참조):**
```cpp
for (int v = 1; v < img->height; v++) {
    for (int u = 1; u < img->width; u++) {
        b[v][u] = ... + b[v-1][u] + b[v][u-1] - b[v-1][u-1];
        //        ↑ b[v], b[v-1] 포인터를 매 u마다 역참조
    }
}
```

**변경 후 (연속 배열 + 행 포인터 1회 계산):**
```cpp
for (int v = 1; v < H; v++) {
    int* bRow  = tableB + v * W;       // 현재 행 포인터 (1회 계산)
    int* bPrev = tableB + (v-1) * W;   // 이전 행 포인터 (1회 계산)
    uchar* srcRow = (uchar*)(img->imageData + v * img->widthStep);

    for (int u = 1; u < W; u++) {
        bRow[u] = srcRow[u * nCh + 0] + bPrev[u] + bRow[u-1] - bPrev[u-1];
    }
}
```

- 이중 포인터 역참조(`b[v][u]`) → 단일 포인터 배열 인덱스(`bRow[u]`)
- CPU가 `bRow`, `bPrev` 포인터를 레지스터에 유지 가능

---

## 4. 최적화 전후 비교표

| 항목 | 현재 | 최적화 후 | 개선 |
|------|------|-----------|------|
| **알고리즘 시간복잡도** | O(W×H) | O(W×H) | 동일 (이미 최적) |
| **공간복잡도** | O(W×H) 3채널 | O(W×H) 3채널 | 동일 |
| **malloc 호출 수** | 3H + 3 | 3 | H배 감소 |
| **메모리 연속성** | 비연속 (행별) | 연속 블록 | 캐시 히트율 향상 |
| **setTable 픽셀 읽기** | W×H × cvGet2D | W×H × 포인터 접근 | 수십 배 빠름 |
| **결과 쓰기** | W×H × cvSet2D | W×H × 포인터 쓰기 | 수십 배 빠름 |
| **k/2 계산** | W×H 회 나눗셈 | 1회 | W×H 배 감소 |
| **cvGetSize 호출** | W×H 회 | 0회 | 완전 제거 |
| **getSmooth 함수 호출** | W×H 회 | 0회 (인라인) | 스택 오버헤드 제거 |
| **경계 처리 정확도** | 일부 경우 버그 | clamp로 항상 정확 | 정확도 개선 |

---

## 5. 우선순위 권고

| 우선순위 | 최적화 항목 | 난이도 | 예상 효과 |
|----------|-------------|--------|-----------|
| ★★★ 최우선 | [2] imageData 직접 접근 | 낮음 | **압도적 (수 배~수십 배)** |
| ★★★ 최우선 | [1] 연속 메모리 블록 | 낮음 | 높음 (캐시 미스 감소) |
| ★★☆ 권장 | [3] 반복 계산 제거 | 낮음 | 중간 |
| ★★☆ 권장 | [5] 경계 처리 수정 | 낮음 | 중간 (정확도 개선 포함) |
| ★☆☆ 선택 | [4] 함수 인라인화 | 중간 | 낮음~중간 |
| ★☆☆ 선택 | [6] 행 포인터 캐싱 | 낮음 | 낮음 |

> **핵심 요약:**  
> HW3.cpp의 알고리즘(Integral Image)은 이미 시간복잡도 O(W×H)로 최적이다.  
> 실제 성능 병목은 `cvGet2D`/`cvSet2D`의 함수 오버헤드와 비연속 메모리 레이아웃이다.  
> `imageData` 직접 접근과 연속 메모리 할당만으로도 수 배의 실제 실행 속도 향상을 기대할 수 있다.
