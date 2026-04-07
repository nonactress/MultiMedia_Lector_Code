#include <opencv2/opencv.hpp>
#include <windows.h> // Windows 멀티스레드 함수 사용을 위한 헤더
#include <cstring>   // strlen 사용을 위한 헤더
#include <stdio.h>


/**
 * @struct OptimalOffset
 * @brief 이미지 정렬을 위한 최적의 이동 거리(x, y)를 저장하는 구조체
 */
typedef struct {
    int xOffset;
    int yOffset;
} OptimalOffset;

// 함수 전방 선언
OptimalOffset alignB(IplImage* greenChannel, IplImage* blueChannel);
OptimalOffset alignR(IplImage* greenChannel, IplImage* redChannel);

/**
 * @struct AlignBThreadData
 * @brief Blue 채널 정렬 스레드에 전달할 데이터 구조체
 */
typedef struct {
    IplImage* greenChannel;
    IplImage* blueChannel;
    OptimalOffset result;
} AlignBThreadData;

/**
 * @struct AlignRThreadData
 * @brief Red 채널 정렬 스레드에 전달할 데이터 구조체
 */
typedef struct {
    IplImage* greenChannel;
    IplImage* redChannel;
    OptimalOffset result;
} AlignRThreadData;

/**
 * @brief Blue 채널 정렬을 수행하는 스레드 함수
 */
DWORD WINAPI alignBThread(LPVOID param) {
    AlignBThreadData* data = (AlignBThreadData*)param;
    if (data != NULL) {
        data->result = alignB(data->greenChannel, data->blueChannel);
    }
    return 0;
}

/**
 * @brief Red 채널 정렬을 수행하는 스레드 함수
 */
DWORD WINAPI alignRThread(LPVOID param) {
    AlignRThreadData* data = (AlignRThreadData*)param;
    if (data != NULL) {
        data->result = alignR(data->greenChannel, data->redChannel);
    }
    return 0;
}

/**
 * @brief SSD(Sum of Squared Differences) 계산 함수
 * 이미지 중앙 영역(1/4 ~ 3/4 지점)만 사용하여 계산 속도를 높임
 */
double calculateSSD(IplImage* base, IplImage* target, int dx, int dy) {
    // 변수 선언을 함수 상단으로 명시적으로 분리
    int width = base->width;
    int height = base->height;
    int startX = width / 4;
    int endX = width * 3 / 4;
    int startY = height / 4;
    int endY = height * 3 / 4;

    double sum = 0.0;
    int count = 0;
    int x, y;

    // 이동된 좌표가 이미지 범위를 벗어나는지 확인
    if ((startX + dx) < 0 || (endX - 1 + dx) >= width ||
        (startY + dy) < 0 || (endY - 1 + dy) >= height) {
        return DBL_MAX;
    }

    const uchar* baseData = (const uchar*)base->imageData;
    const uchar* targetData = (const uchar*)target->imageData;
    int baseStep = base->widthStep;
    int targetStep = target->widthStep;

    for (y = startY; y < endY; y++) {
        const uchar* baseRow = baseData + (y * baseStep);
        const uchar* targetRow = targetData + ((y + dy) * targetStep);

        for (x = startX; x < endX; x++) {
            int base_val = (int)baseRow[x];
            int target_val = (int)targetRow[x + dx];
            int diff = base_val - target_val;

            sum += (double)diff * diff;
            count++;
        }
    }

    if (count == 0) return DBL_MAX;
    return sum / (double)count;
}

/**
 * @brief 2단계 탐색을 통한 Blue 채널 정렬
 */
OptimalOffset alignB(IplImage* greenChannel, IplImage* blueChannel) {
    int SEARCH_RANGE_X = 15;
    int SEARCH_RANGE_Y = 35;
    int alignX = 0;
    int alignY = 0;
    double minSsd = DBL_MAX;
    int x, y;

    // 1단계: 수직(Y) 방향 탐색 (X는 0으로 고정)
    printf("alignB by Y\n");
    for (y = -SEARCH_RANGE_Y; y <= SEARCH_RANGE_Y; y++) {
        double ssd = calculateSSD(greenChannel, blueChannel, 0, y);
        if (ssd < minSsd) {
            minSsd = ssd;
            alignY = y;
        }
    }

    minSsd = DBL_MAX;

    // 2단계: 수평(X) 방향 탐색 (찾은 Y 고정)
    printf("alignB by X\n");
    for (x = -SEARCH_RANGE_X; x <= SEARCH_RANGE_X; x++) {
        double ssd = calculateSSD(greenChannel, blueChannel, x, alignY);
        if (ssd < minSsd) {
            minSsd = ssd;
            alignX = x;
        }
    }

    OptimalOffset result;
    result.xOffset = alignX;
    result.yOffset = alignY;
    return result;
}

/**
 * @brief 2단계 탐색을 통한 Red 채널 정렬
 */
OptimalOffset alignR(IplImage* greenChannel, IplImage* redChannel) {
    int SEARCH_RANGE_X = 15;
    int SEARCH_RANGE_Y = 35;
    int alignX = 0;
    int alignY = 0;
    double minSsd = DBL_MAX;
    int x, y;

    printf("alignR by y\n");
    for (y = -SEARCH_RANGE_Y; y <= SEARCH_RANGE_Y; y++) {
        double ssd = calculateSSD(greenChannel, redChannel, 0, y);
        if (ssd < minSsd) {
            minSsd = ssd;
            alignY = y;
        }
    }

    minSsd = DBL_MAX;

    printf("alignR by x\n");
    for (x = -SEARCH_RANGE_X; x <= SEARCH_RANGE_X; x++) {
        double ssd = calculateSSD(greenChannel, redChannel, x, alignY);
        if (ssd < minSsd) {
            minSsd = ssd;
            alignX = x;
        }
    }

    OptimalOffset result;
    result.xOffset = alignX;
    result.yOffset = alignY;
    return result;
}

int main() {
    printf("TestCV\n");
    char imagePath[256];
    printf("Input File Name : ");
    if (fgets(imagePath, sizeof(imagePath), stdin) == NULL) return -1;

    int len = (int)strlen(imagePath);
    if (len > 0 && imagePath[len - 1] == '\n') {
        imagePath[len - 1] = '\0';
    }

    IplImage* src = cvLoadImage(imagePath);

    int width = src->width;
    int height = src->height / 3;

    IplImage* dest = cvCreateImage(cvSize(width, height), 8, 3);
    IplImage* blueChannel = cvCreateImage(cvSize(width, height), 8, 1);
    IplImage* greenChannel = cvCreateImage(cvSize(width, height), 8, 1);
    IplImage* redChannel = cvCreateImage(cvSize(width, height), 8, 1);

    const uchar* srcData = (const uchar*)src->imageData;
    int srcStep = src->widthStep;
    uchar* bData = (uchar*)blueChannel->imageData;
    uchar* gData = (uchar*)greenChannel->imageData;
    uchar* rData = (uchar*)redChannel->imageData;
    int dStep = blueChannel->widthStep;

    // 이미지 채널 분리
    for (int y = 0; y < height; y++) {
        const uchar* sB = srcData + (y * srcStep);
        const uchar* sG = srcData + ((y + height) * srcStep);
        const uchar* sR = srcData + ((y + height * 2) * srcStep);

        uchar* dB = bData + (y * dStep);
        uchar* dG = gData + (y * dStep);
        uchar* dR = rData + (y * dStep);

        for (int x = 0; x < width; x++) {
            dB[x] = sB[x * 3 + 0];
            dG[x] = sG[x * 3 + 1];
            dR[x] = sR[x * 3 + 2];
        }
    }

    AlignBThreadData dataB;
    dataB.greenChannel = greenChannel;
    dataB.blueChannel = blueChannel;

    AlignRThreadData dataR;
    dataR.greenChannel = greenChannel;
    dataR.redChannel = redChannel;

    clock_t start = clock();

    // 멀티스레드 실행
    HANDLE hB = CreateThread(NULL, 0, alignBThread, &dataB, 0, NULL);
    HANDLE hR = CreateThread(NULL, 0, alignRThread, &dataR, 0, NULL);

    if (hB && hR) {
        WaitForSingleObject(hB, INFINITE);
        WaitForSingleObject(hR, INFINITE);
        CloseHandle(hB);
        CloseHandle(hR);
    }

    double duration = (double)(clock() - start) / CLOCKS_PER_SEC;

    OptimalOffset offB = dataB.result;
    OptimalOffset offR = dataR.result;

    // 채널 병합
    int bStep = blueChannel->widthStep;
    int gStep = greenChannel->widthStep;
    int rStep = redChannel->widthStep;
    int resStep = dest->widthStep;
    uchar* resData = (uchar*)dest->imageData;

    for (int y = 0; y < height; y++) {
        uchar* row = resData + (y * resStep);
        for (int x = 0; x < width; x++) {
            uchar gVal = ((uchar*)greenChannel->imageData + y * gStep)[x];

            int bx = x + offB.xOffset, by = y + offB.yOffset;
            uchar bVal = (bx >= 0 && bx < width && by >= 0 && by < height) ?
                ((uchar*)blueChannel->imageData + by * bStep)[bx] : 0;

            int rx = x + offR.xOffset, ry = y + offR.yOffset;
            uchar rVal = (rx >= 0 && rx < width && ry >= 0 && ry < height) ?
                ((uchar*)redChannel->imageData + ry * rStep)[rx] : 0;

            row[x * 3 + 0] = bVal;
            row[x * 3 + 1] = gVal;
            row[x * 3 + 2] = rVal;
        }
    }

    cvShowImage("Original", src);
    cvShowImage("Aligned", dest);
    cvWaitKey(0);

    cvReleaseImage(&src);
    cvReleaseImage(&dest);
    cvReleaseImage(&blueChannel);
    cvReleaseImage(&greenChannel);
    cvReleaseImage(&redChannel);

    return 0;
}