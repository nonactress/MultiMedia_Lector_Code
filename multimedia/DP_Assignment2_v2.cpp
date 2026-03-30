/*
    ========================================
    DP_Assignment2_v2.cpp
    Y축 분산 기반 골짜기 탐색 + SSD 채널 정렬 버전
    ========================================

    [변경 요약]
    - 기존: 평균 밝기의 1차/2차 차분으로 채널 경계 검출 (피사체 밝기를 경계로 오인)
    - 신규: 행별 분산 프로파일 계산 → 평활화 → 저분산 구간(Valley) 탐색으로 경계 검출
    - 추가: 가운데(Green) 채널 기준 SSD 탐색으로 채널 간 어긋남(misalignment) 보정

    [처리 흐름]
    1. calculateVarianceY()    : 각 행의 픽셀 밝기 분산 계산
    2. smoothVariance()        : 슬라이딩 윈도우 평균으로 분산 배열 평활화
    3. findVarianceYBoundaries(): 평활화된 배열에서 저분산 구간(Valley) 2개 탐색
                                  → 두 구간의 중심을 y1, y2(채널 경계)로 결정
    4. cropChannel()           : y1, y2 기준으로 3개 채널 이미지 분리
    5. alignAndMerge()         : Green 기준 ±SEARCH_RANGE 탐색(SSD),
                                  최적 오프셋으로 Blue/Red 정렬 후 RGB 합성

    [제약]
    - cvGet2D / cvSet2D + 기본 루프만 사용 (새 OpenCV 함수 사용 금지)
*/

#include <opencv2/opencv.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <climits>

// ── 상수 ──────────────────────────────────────────────────────────────────────
const int    TOP_K            = 100;
const int    MAX_BRIGHTNESS   = 255;
const int    RGB_CHANNELS     = 3;
const int    THRESHOLD        = 10;          // (기존 코드 호환용)
const int    SMOOTH_WINDOW    = 7;           // 분산 평활화 윈도우 크기 (5~11, 홀수)
const int    SEARCH_RANGE     = 15;          // 채널 정렬 탐색 범위 ±픽셀
const double VAR_THRESH_RATIO = 0.4;         // 저분산 임계: 전체 평균의 40%
const int    MAX_RUNS         = 100;         // 최대 연속 저분산 구간 수

// ── 구조체 ────────────────────────────────────────────────────────────────────
/*
    ImageAnalysis
    이미지 분석에 필요한 배열과 경계 위치 정보를 담는 구조체.
    v2에서 varY, smoothVarY 필드 추가.
*/
struct ImageAnalysis {
    int*    avgX;       // 열 단위 평균 밝기
    int*    avgY;       // 행 단위 평균 밝기
    int*    diffX;      // X축 1차 차분
    int*    diffY;      // Y축 1차 차분
    int*    diff2X;     // X축 2차 차분
    int*    diff2Y;     // Y축 2차 차분
    double* varY;       // [신규] 행별 픽셀 밝기 분산
    double* smoothVarY; // [신규] 평활화된 분산
    int sizeX, sizeY;
    int leftX, rightX;  // X축 경계 (기존 방식 유지)
    int topY, bottomY;  // [갱신] Valley 탐색으로 결정된 채널 경계 Y좌표
};

// ── 전방 선언 ─────────────────────────────────────────────────────────────────
ImageAnalysis* allocateAnalysis(int sizeX, int sizeY);
void           freeAnalysis(ImageAnalysis* analysis);
void           showImageFit(const char* windowName, IplImage* img, int maxWidth, int maxHeight);
int            brightnessAvgX(IplImage* img, int* avgX);
int            brightnessAvgY(IplImage* img, int* avgY);
int*           findEdge(int* arr, int* diff, int size);
void           findSecondDiff(int* diff, int* diff2, int size);
void           findBoundary(int* diff, int size, int* leftOut, int* rightOut);
void           drawLineByX(IplImage* img, int* pos, int size);
void           drawLineByY(IplImage* img, int* pos, int size);
void           calculateVarianceY(IplImage* img, double* varY);
void           smoothVariance(double* src, double* dst, int size, int window);
void           findVarianceYBoundaries(double* smoothed, int size, int* y1Out, int* y2Out);
IplImage*      cropChannel(IplImage* src, int yStart, int yEnd);
long long      computeSSD(IplImage* ref, IplImage* src, int dy, int dx);
IplImage*      alignAndMerge(IplImage* chB, IplImage* chG, IplImage* chR);
void           processImageAnalysis(IplImage* img, ImageAnalysis* analysis);

// ── allocateAnalysis ──────────────────────────────────────────────────────────
/*
    목적: ImageAnalysis 구조체 및 모든 배열 동적 할당
    v2 변경: varY(double), smoothVarY(double) 배열 추가 할당
*/
ImageAnalysis* allocateAnalysis(int sizeX, int sizeY)
{
    ImageAnalysis* a = (ImageAnalysis*)malloc(sizeof(ImageAnalysis));
    a->avgX      = (int*)   malloc(sizeof(int)    * sizeX);
    a->avgY      = (int*)   malloc(sizeof(int)    * sizeY);
    a->diffX     = (int*)   malloc(sizeof(int)    * (sizeX - 1));
    a->diffY     = (int*)   malloc(sizeof(int)    * (sizeY - 1));
    a->diff2X    = (int*)   malloc(sizeof(int)    * (sizeX - 2));
    a->diff2Y    = (int*)   malloc(sizeof(int)    * (sizeY - 2));
    a->varY      = (double*)malloc(sizeof(double) * sizeY); // [신규]
    a->smoothVarY= (double*)malloc(sizeof(double) * sizeY); // [신규]
    a->sizeX     = sizeX;
    a->sizeY     = sizeY;
    a->leftX = a->rightX = a->topY = a->bottomY = -1;
    return a;
}

// ── freeAnalysis ──────────────────────────────────────────────────────────────
void freeAnalysis(ImageAnalysis* a)
{
    if (!a) return;
    free(a->avgX);
    free(a->avgY);
    free(a->diffX);
    free(a->diffY);
    free(a->diff2X);
    free(a->diff2Y);
    free(a->varY);       // [신규]
    free(a->smoothVarY); // [신규]
    free(a);
}

// ── showImageFit ──────────────────────────────────────────────────────────────
/*
    목적: 이미지를 maxWidth x maxHeight 내에 맞춰 윈도우에 표시
*/
void showImageFit(const char* windowName, IplImage* img,
                  int maxWidth = 1200, int maxHeight = 800)
{
    float scaleW = (float)maxWidth  / img->width;
    float scaleH = (float)maxHeight / img->height;
    float scale  = (scaleW < scaleH) ? scaleW : scaleH;

    if (scale < 1.0f) {
        IplImage* resized = cvCreateImage(
            cvSize((int)(img->width * scale), (int)(img->height * scale)),
            img->depth, img->nChannels);
        cvResize(img, resized, CV_INTER_LINEAR);
        cvShowImage(windowName, resized);
        cvResizeWindow(windowName, resized->width, resized->height);
        cvReleaseImage(&resized);
    } else {
        cvShowImage(windowName, img);
        cvResizeWindow(windowName, img->width, img->height);
    }
}

// ── brightnessAvgX ────────────────────────────────────────────────────────────
/*
    목적: 각 열(X축)의 평균 밝기값 계산 및 그래프 시각화
*/
int brightnessAvgX(IplImage* img, int* avgX)
{
    CvSize size = cvGetSize(img);
    IplImage* dst = cvCreateImage(cvSize(size.width, MAX_BRIGHTNESS), 8, RGB_CHANNELS);
    cvSet(dst, cvScalar(0, 0, 0));

    for (int x = 0; x < size.width; x++) {
        int avg = 0;
        for (int y = 0; y < size.height; y++) {
            CvScalar c = cvGet2D(img, y, x);
            avg += (int)((c.val[0] + c.val[1] + c.val[2]) / 3);
        }
        avg /= size.height;
        avgX[x] = avg;
        if (avg >= 0 && avg < MAX_BRIGHTNESS)
            cvSet2D(dst, MAX_BRIGHTNESS - avg - 1, x, cvScalar(255, 255, 255));
    }

    showImageFit("X Brightness", dst);
    cvReleaseImage(&dst);
    return size.width;
}

// ── brightnessAvgY ────────────────────────────────────────────────────────────
/*
    목적: 각 행(Y축)의 평균 밝기값 계산 및 그래프 시각화
*/
int brightnessAvgY(IplImage* img, int* avgY)
{
    CvSize size = cvGetSize(img);
    IplImage* dst = cvCreateImage(cvSize(MAX_BRIGHTNESS, size.height), 8, RGB_CHANNELS);
    cvSet(dst, cvScalar(0, 0, 0));

    for (int y = 0; y < size.height; y++) {
        int avg = 0;
        for (int x = 0; x < size.width; x++) {
            CvScalar c = cvGet2D(img, y, x);
            avg += (int)((c.val[0] + c.val[1] + c.val[2]) / 3);
        }
        avg /= size.width;
        avgY[y] = avg;
        if (avg >= 0 && avg < MAX_BRIGHTNESS)
            cvSet2D(dst, y, avg, cvScalar(255, 255, 255));
    }

    showImageFit("Y Brightness", dst);
    cvReleaseImage(&dst);
    return size.height;
}

// ── findEdge ──────────────────────────────────────────────────────────────────
/*
    목적: 평균 밝기 배열의 1차 차분 계산
    diff[i] = arr[i+1] - arr[i]
*/
int* findEdge(int* arr, int* diff, int size)
{
    for (int i = 1; i < size; i++)
        diff[i - 1] = arr[i] - arr[i - 1];
    return diff;
}

// ── findSecondDiff ────────────────────────────────────────────────────────────
/*
    목적: 1차 차분의 2차 차분 계산 (X축 분석용, 기존 유지)
*/
void findSecondDiff(int* diff, int* diff2, int size)
{
    if (size < 2) return;
    for (int i = 0; i < size - 1; i++)
        diff2[i] = diff[i + 1] - diff[i];
}

// ── findBoundary ──────────────────────────────────────────────────────────────
/*
    목적: 1차 차분에서 최대/최소 위치를 찾아 경계 검출 (X축 분석용, 기존 유지)
*/
void findBoundary(int* diff, int size, int* leftOut, int* rightOut)
{
    int maxPos = -1, maxVal = INT_MIN;
    int minPos = -1, minVal = INT_MAX;

    for (int i = 0; i < size; i++) {
        if (diff[i] > maxVal) { maxVal = diff[i]; maxPos = i; }
        if (diff[i] < minVal) { minVal = diff[i]; minPos = i; }
    }
    *leftOut  = minPos;
    *rightOut = maxPos;
}

// ── drawLineByX ───────────────────────────────────────────────────────────────
void drawLineByX(IplImage* img, int* pos, int size)
{
    if (!img || !pos) return;
    for (int i = 0; i < size; i++) {
        if (pos[i] >= 0 && pos[i] < img->width) {
            for (int y = 0; y < img->height; y++)
                cvSet2D(img, y, pos[i], cvScalar(255, 0, 0));
        }
    }
}

// ── drawLineByY ───────────────────────────────────────────────────────────────
void drawLineByY(IplImage* img, int* pos, int size)
{
    if (!img || !pos) return;
    for (int i = 0; i < size; i++) {
        if (pos[i] >= 0 && pos[i] < img->height) {
            for (int x = 0; x < img->width; x++)
                cvSet2D(img, pos[i], x, cvScalar(0, 255, 0));
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// [신규] calculateVarianceY
// ═══════════════════════════════════════════════════════════════════════════════
/*
    목적: 각 행(row)의 픽셀 밝기 분산 계산

    알고리즘:
      for 각 행 y:
        1) 행 평균 brightness = Σ(픽셀 밝기) / width
        2) 분산 = Σ((픽셀 밝기 - 평균)²) / width
        → varY[y] = 분산

    채널 경계(검은 띠 또는 균일한 영역)는 분산이 매우 낮게 나타남.
    반면 피사체가 있는 행은 밝기 변화가 커서 분산이 높음.

    입력: img  - 분석할 이미지
          varY - 행별 분산을 저장할 배열 (크기: img->height)
*/
void calculateVarianceY(IplImage* img, double* varY)
{
    int width  = img->width;
    int height = img->height;

    for (int y = 0; y < height; y++) {
        // 1단계: 행 평균 계산
        double sum = 0.0;
        for (int x = 0; x < width; x++) {
            CvScalar c = cvGet2D(img, y, x);
            double brightness = (c.val[0] + c.val[1] + c.val[2]) / 3.0;
            sum += brightness;
        }
        double mean = sum / width;

        // 2단계: 분산 계산
        double sqSum = 0.0;
        for (int x = 0; x < width; x++) {
            CvScalar c = cvGet2D(img, y, x);
            double brightness = (c.val[0] + c.val[1] + c.val[2]) / 3.0;
            double diff = brightness - mean;
            sqSum += diff * diff;
        }
        varY[y] = sqSum / width;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// [신규] smoothVariance
// ═══════════════════════════════════════════════════════════════════════════════
/*
    목적: 분산 배열을 슬라이딩 윈도우 평균으로 평활화

    알고리즘:
      for 각 인덱스 i:
        half = window / 2
        [i-half, i+half] 범위(경계 클램핑)의 평균 → dst[i]

    분산 프로파일의 노이즈를 줄여 Valley 탐색의 안정성을 높임.

    입력: src    - 원본 분산 배열
          dst    - 평활화된 분산을 저장할 배열
          size   - 배열 크기
          window - 윈도우 크기 (홀수 권장: 5~11, 기본값 SMOOTH_WINDOW=7)
*/
void smoothVariance(double* src, double* dst, int size, int window)
{
    int half = window / 2;
    for (int i = 0; i < size; i++) {
        int   start = (i - half > 0)        ? (i - half) : 0;
        int   end   = (i + half < size - 1) ? (i + half) : (size - 1);
        double sum  = 0.0;
        int   cnt   = 0;
        for (int j = start; j <= end; j++) {
            sum += src[j];
            cnt++;
        }
        dst[i] = (cnt > 0) ? (sum / cnt) : src[i];
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// [신규] findVarianceYBoundaries  ← 핵심 변경
// ═══════════════════════════════════════════════════════════════════════════════
/*
    목적: 평활화된 분산 배열에서 가장 긴 저분산 구간(Valley) 2개를 탐색하여
          채널 경계 y1, y2를 결정

    알고리즘:
      1. 전체 분산의 평균(mean) 계산
      2. threshold = mean * VAR_THRESH_RATIO (기본 0.4)
      3. smoothed[i] < threshold 인 연속 구간을 Run으로 기록
         Run = {start, end, length}
      4. Run을 길이 내림차순 정렬 (단순 선택 정렬)
      5. 상위 2개 Run의 중심을 y1, y2로 반환 (y1 < y2 보장)
      6. Run이 2개 미만이면 Fallback: size/3, size*2/3

    채널 사이의 검은 경계선 또는 균일한 여백은 분산이 매우 낮게 나타남.
    따라서 가장 넓고 낮은 두 구간의 중심이 채널 경계선에 해당함.

    입력: smoothed - 평활화된 분산 배열
          size     - 배열 크기
          y1Out    - 첫 번째 경계 Y좌표 출력
          y2Out    - 두 번째 경계 Y좌표 출력
*/
void findVarianceYBoundaries(double* smoothed, int size, int* y1Out, int* y2Out)
{
    // Fallback 기본값
    *y1Out = size / 3;
    *y2Out = size * 2 / 3;

    // 1. 전체 평균 계산
    double total = 0.0;
    for (int i = 0; i < size; i++)
        total += smoothed[i];
    double mean = total / size;

    double threshold = mean * VAR_THRESH_RATIO;

    // 2. 연속 저분산 구간(Run) 수집
    struct Run { int start, end, length; };
    Run runs[MAX_RUNS];
    int runCount = 0;

    int i = 0;
    while (i < size && runCount < MAX_RUNS) {
        if (smoothed[i] < threshold) {
            int start = i;
            while (i < size && smoothed[i] < threshold)
                i++;
            int end = i - 1;
            runs[runCount].start  = start;
            runs[runCount].end    = end;
            runs[runCount].length = end - start + 1;
            runCount++;
        } else {
            i++;
        }
    }

    if (runCount < 2) {
        printf("[Valley] Run < 2개 (runCount=%d), Fallback 사용: y1=%d y2=%d\n",
               runCount, *y1Out, *y2Out);
        return;
    }

    // 3. 길이 내림차순 선택 정렬 (최대 2단계만 필요)
    for (int a = 0; a < 2 && a < runCount - 1; a++) {
        int maxIdx = a;
        for (int b = a + 1; b < runCount; b++) {
            if (runs[b].length > runs[maxIdx].length)
                maxIdx = b;
        }
        if (maxIdx != a) {
            Run tmp    = runs[a];
            runs[a]    = runs[maxIdx];
            runs[maxIdx] = tmp;
        }
    }

    // 4. 상위 2개 Run의 중심을 y1, y2로 결정
    int centerA = (runs[0].start + runs[0].end) / 2;
    int centerB = (runs[1].start + runs[1].end) / 2;

    *y1Out = (centerA < centerB) ? centerA : centerB;
    *y2Out = (centerA < centerB) ? centerB : centerA;

    printf("[Valley] 탐색 완료: threshold=%.2f, Run 수=%d\n", threshold, runCount);
    printf("  1번 Run: [%d ~ %d] 길이=%d, 중심=%d\n",
           runs[0].start, runs[0].end, runs[0].length, centerA);
    printf("  2번 Run: [%d ~ %d] 길이=%d, 중심=%d\n",
           runs[1].start, runs[1].end, runs[1].length, centerB);
    printf("  → y1=%d, y2=%d\n", *y1Out, *y2Out);
}

// ═══════════════════════════════════════════════════════════════════════════════
// [신규] cropChannel
// ═══════════════════════════════════════════════════════════════════════════════
/*
    목적: 원본 이미지에서 [yStart, yEnd) 행 범위를 새 IplImage로 복사

    cvGet2D / cvSet2D + 기본 루프만 사용.

    입력: src    - 원본 이미지
          yStart - 시작 행 (포함)
          yEnd   - 끝 행 (미포함)
    반환: 잘라낸 이미지 (실패 시 NULL)
*/
IplImage* cropChannel(IplImage* src, int yStart, int yEnd)
{
    if (!src || yStart < 0 || yEnd > src->height || yStart >= yEnd)
        return NULL;

    int newH = yEnd - yStart;
    int newW = src->width;
    IplImage* dst = cvCreateImage(cvSize(newW, newH), src->depth, src->nChannels);

    for (int y = 0; y < newH; y++) {
        for (int x = 0; x < newW; x++) {
            CvScalar c = cvGet2D(src, y + yStart, x);
            cvSet2D(dst, y, x, c);
        }
    }
    return dst;
}

// ═══════════════════════════════════════════════════════════════════════════════
// [신규] computeSSD
// ═══════════════════════════════════════════════════════════════════════════════
/*
    목적: ref 이미지 대비 src 이미지의 오프셋(dy, dx) 적용 SSD 계산

    ref[y][x] 와 src[y + dy][x + dx] 간의 제곱 차이 합산.
    두 이미지의 유효 겹침 영역만 계산함.

    SSD가 작을수록 두 채널이 잘 정렬되어 있음을 의미.

    입력: ref - 기준 이미지 (Green 채널)
          src - 정렬 대상 이미지 (Blue 또는 Red 채널)
          dy  - 수직 오프셋 (src를 dy만큼 이동한다고 가정)
          dx  - 수평 오프셋
    반환: SSD 값 (long long)
*/
long long computeSSD(IplImage* ref, IplImage* src, int dy, int dx)
{
    int refH = ref->height, refW = ref->width;
    int srcH = src->height, srcW = src->width;

    long long ssd = 0;
    for (int y = 0; y < refH; y++) {
        int sy = y + dy;
        if (sy < 0 || sy >= srcH) continue;

        for (int x = 0; x < refW; x++) {
            int sx = x + dx;
            if (sx < 0 || sx >= srcW) continue;

            CvScalar cr = cvGet2D(ref, y,  x);
            CvScalar cs = cvGet2D(src, sy, sx);

            // 그레이스케일 플레이트이므로 채널 0만 사용
            double diff = cr.val[0] - cs.val[0];
            ssd += (long long)(diff * diff);
        }
    }
    return ssd;
}

// ═══════════════════════════════════════════════════════════════════════════════
// [신규] alignAndMerge  ← 가장 중요
// ═══════════════════════════════════════════════════════════════════════════════
/*
    목적: Green(중앙) 채널을 기준으로 Blue/Red 채널을 정렬한 뒤 RGB 합성

    알고리즘:
      1. Blue 정렬:
         for dy in [-SEARCH_RANGE, +SEARCH_RANGE]:
           for dx in [-SEARCH_RANGE, +SEARCH_RANGE]:
             ssd = computeSSD(chG, chB, dy, dx)
             최솟값 갱신 → bestDy_B, bestDx_B
      2. Red 정렬: 동일 방식 → bestDy_R, bestDx_R
      3. 합성:
         dst[y][x] = (B=chB[y+bestDy_B][x+bestDx_B],
                      G=chG[y][x],
                      R=chR[y+bestDy_R][x+bestDx_R])
         범위를 벗어난 샘플은 0으로 처리.

    입력: chB - Blue 채널 이미지 (상단 1/3)
          chG - Green 채널 이미지 (중간 1/3, 기준)
          chR - Red 채널 이미지 (하단 1/3)
    반환: 정렬·합성된 컬러 IplImage* (호출자가 cvReleaseImage로 해제)
*/
IplImage* alignAndMerge(IplImage* chB, IplImage* chG, IplImage* chR)
{
    int H = chG->height;
    int W = chG->width;

    // ── Blue 최적 오프셋 탐색 ────────────────────────────────────────────────
    long long bestSSD_B = LLONG_MAX;
    int bestDy_B = 0, bestDx_B = 0;

    printf("[Align] Blue 탐색 시작 (±%d)...\n", SEARCH_RANGE);
    for (int dy = -SEARCH_RANGE; dy <= SEARCH_RANGE; dy++) {
        for (int dx = -SEARCH_RANGE; dx <= SEARCH_RANGE; dx++) {
            long long ssd = computeSSD(chG, chB, dy, dx);
            if (ssd < bestSSD_B) {
                bestSSD_B = ssd;
                bestDy_B  = dy;
                bestDx_B  = dx;
            }
        }
    }
    printf("[Align] Blue 최적 오프셋: dy=%d, dx=%d (SSD=%lld)\n",
           bestDy_B, bestDx_B, bestSSD_B);

    // ── Red 최적 오프셋 탐색 ─────────────────────────────────────────────────
    long long bestSSD_R = LLONG_MAX;
    int bestDy_R = 0, bestDx_R = 0;

    printf("[Align] Red 탐색 시작 (±%d)...\n", SEARCH_RANGE);
    for (int dy = -SEARCH_RANGE; dy <= SEARCH_RANGE; dy++) {
        for (int dx = -SEARCH_RANGE; dx <= SEARCH_RANGE; dx++) {
            long long ssd = computeSSD(chG, chR, dy, dx);
            if (ssd < bestSSD_R) {
                bestSSD_R = ssd;
                bestDy_R  = dy;
                bestDx_R  = dx;
            }
        }
    }
    printf("[Align] Red 최적 오프셋: dy=%d, dx=%d (SSD=%lld)\n",
           bestDy_R, bestDx_R, bestSSD_R);

    // ── RGB 합성 ──────────────────────────────────────────────────────────────
    IplImage* dst = cvCreateImage(cvSize(W, H), 8, RGB_CHANNELS);
    cvSet(dst, cvScalar(0, 0, 0));

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            // Green (기준, 이동 없음)
            CvScalar g = cvGet2D(chG, y, x);

            // Blue (오프셋 적용, 범위 클램핑)
            double bVal = 0.0;
            int by = y + bestDy_B, bx = x + bestDx_B;
            if (by >= 0 && by < chB->height && bx >= 0 && bx < chB->width) {
                CvScalar b = cvGet2D(chB, by, bx);
                bVal = b.val[0];
            }

            // Red (오프셋 적용, 범위 클램핑)
            double rVal = 0.0;
            int ry = y + bestDy_R, rx = x + bestDx_R;
            if (ry >= 0 && ry < chR->height && rx >= 0 && rx < chR->width) {
                CvScalar r = cvGet2D(chR, ry, rx);
                rVal = r.val[0];
            }

            // OpenCV 기본 순서: (B, G, R)
            cvSet2D(dst, y, x, cvScalar(bVal, g.val[0], rVal));
        }
    }
    return dst;
}

// ═══════════════════════════════════════════════════════════════════════════════
// processImageAnalysis  (v2: Y축을 분산 기반 파이프라인으로 교체)
// ═══════════════════════════════════════════════════════════════════════════════
/*
    목적: 이미지 전체 분석 수행

    [X축 분석] — 기존 방식 유지
      brightnessAvgX → findEdge → findBoundary

    [Y축 분석] — v2 신규 파이프라인
      brightnessAvgY (시각화)
      calculateVarianceY  → 행별 분산 계산
      smoothVariance      → 윈도우 평활화
      findVarianceYBoundaries → Valley 탐색 → topY, bottomY 결정
*/
void processImageAnalysis(IplImage* img, ImageAnalysis* analysis)
{
    // X축 분석 (기존 방식 유지)
    brightnessAvgX(img, analysis->avgX);
    findEdge(analysis->avgX, analysis->diffX, analysis->sizeX);
    findBoundary(analysis->diffX, analysis->sizeX - 1,
                 &analysis->leftX, &analysis->rightX);
    findSecondDiff(analysis->diffX, analysis->diff2X, analysis->sizeX - 1);

    // Y축 분석 (v2 신규 파이프라인)
    brightnessAvgY(img, analysis->avgY);           // 시각화 유지

    printf("[V2] Y축 분산 프로파일 계산 중...\n");
    calculateVarianceY(img, analysis->varY);

    printf("[V2] 분산 평활화 (윈도우=%d)...\n", SMOOTH_WINDOW);
    smoothVariance(analysis->varY, analysis->smoothVarY,
                   analysis->sizeY, SMOOTH_WINDOW);

    printf("[V2] Valley 탐색 (임계 비율=%.2f)...\n", VAR_THRESH_RATIO);
    findVarianceYBoundaries(analysis->smoothVarY, analysis->sizeY,
                            &analysis->topY, &analysis->bottomY);

    printf("[V2] 채널 경계: topY=%d, bottomY=%d (이미지 높이=%d)\n",
           analysis->topY, analysis->bottomY, analysis->sizeY);
}

// ═══════════════════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════════════════
/*
    실행 순서:
    1. 이미지 로드
    2. processImageAnalysis() — 분산 기반 채널 경계(topY, bottomY) 검출
    3. cropChannel()          — 3개 채널 이미지 분리
    4. alignAndMerge()        — SSD 정렬 후 RGB 합성
    5. 결과 표시 및 메모리 해제
*/
int main()
{
    IplImage* src = cvLoadImage("c:\\MultiMedia\\AS2\\pg1.jpg");
    if (!src) {
        printf("Error: 이미지를 불러올 수 없습니다.\n");
        return -1;
    }

    CvSize size   = cvGetSize(src);
    ImageAnalysis* analysis = allocateAnalysis(size.width, size.height);

    // 1. 분산 기반 채널 경계 검출
    processImageAnalysis(src, analysis);

    int y1 = analysis->topY;
    int y2 = analysis->bottomY;

    // 경계가 유효한지 검증
    if (y1 <= 0 || y2 <= y1 || y2 >= size.height) {
        printf("경계 오류: y1=%d, y2=%d — Fallback 적용\n", y1, y2);
        y1 = size.height / 3;
        y2 = size.height * 2 / 3;
    }

    printf("\n채널 분리: Blue[0~%d], Green[%d~%d], Red[%d~%d]\n",
           y1, y1, y2, y2, size.height);

    // 2. 경계 시각화 (원본 이미지 복사본에 수평선 표시)
    IplImage* preview = cvCreateImage(size, 8, RGB_CHANNELS);
    cvCopy(src, preview);
    int boundaries[2] = { y1, y2 };
    drawLineByY(preview, boundaries, 2);
    showImageFit("Channel Boundaries", preview);
    cvReleaseImage(&preview);

    // 3. 채널 분리 (cropChannel)
    IplImage* chB = cropChannel(src, 0,  y1);
    IplImage* chG = cropChannel(src, y1, y2);
    IplImage* chR = cropChannel(src, y2, size.height);

    if (!chB || !chG || !chR) {
        printf("Error: 채널 분리 실패\n");
        if (chB) cvReleaseImage(&chB);
        if (chG) cvReleaseImage(&chG);
        if (chR) cvReleaseImage(&chR);
        freeAnalysis(analysis);
        cvReleaseImage(&src);
        return -1;
    }

    showImageFit("Blue Channel",  chB);
    showImageFit("Green Channel", chG);
    showImageFit("Red Channel",   chR);

    // 4. SSD 기반 채널 정렬 및 RGB 합성
    printf("\n[Align] 채널 정렬 및 합성 시작...\n");
    IplImage* result = alignAndMerge(chB, chG, chR);

    if (result) {
        showImageFit("Aligned Result", result);
        printf("[Align] 합성 완료.\n");
        cvReleaseImage(&result);
    }

    // 5. 메모리 해제
    cvReleaseImage(&chB);
    cvReleaseImage(&chG);
    cvReleaseImage(&chR);
    freeAnalysis(analysis);

    cvWaitKey();
    cvReleaseImage(&src);
    return 0;
}
