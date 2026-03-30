
#include<opencv2/opencv.hpp>
#include<stdlib.h>
#include<stdio.h>

// Constants
const int TOP_K = 100;
const int MAX_BRIGHTNESS = 255;
const int HIGHLIGHT_COLOR = 100;
const int RGB_CHANNELS = 3;
const int THRESHOLD = 10;  // 2차 차분 피크 검출 임계값

// Image Analysis structure
// 이미지 분석에 필요한 모든 배열과 경계 위치 정보를 담는 구조체
struct ImageAnalysis {
	int* avgX;		// X축(열) 단위의 평균 밝기값
	int* avgY;		// Y축(행) 단위의 평균 밝기값
	int* diffX;		// X축의 1차 차분 (인접 밝기 차이)
	int* diffY;		// Y축의 1차 차분 (인접 밝기 차이)
	int* diff2X;	// X축의 2차 차분 (차분의 차분 = 곡률)
	int* diff2Y;	// Y축의 2차 차분 (차분의 차분 = 곡률)
	int sizeX;		// 이미지 너비 (열 개수)
	int sizeY;		// 이미지 높이 (행 개수)
	int leftX;		// 왼쪽 경계 X (열 인덱스, -1=미검출)
	int rightX;		// 오른쪽 경계 X
	int topY;		// 위쪽 경계 Y (행 인덱스, -1=미검출)
	int bottomY;	// 아래쪽 경계 Y
};

int brightnessAvgX(IplImage* img, int* avgX);
int brightnessAvgY(IplImage* img, int* avgY);
int* findEdge(int* arr, int* diff, int size);
void findSecondDiff(int* diff, int* diff2, int size);
int* getDiffPos(int* diff, int pos[], int size);
void findBoundary(int* diff, int size, int* leftOut, int* rightOut);
void drawLineByX(IplImage* img, int* pos, int size);
void drawLineByY(IplImage* img, int* pos, int size);
void showImageFit(const char* windowName, IplImage* img, int maxWidth, int maxHeight);
ImageAnalysis* allocateAnalysis(int sizeX, int sizeY);
void freeAnalysis(ImageAnalysis* analysis);
void processImageAnalysis(IplImage* img, ImageAnalysis* analysis);
void extractRGBChannels(IplImage* img, CvSize size, int gap, IplImage* dst);
void visualizeResults(IplImage* img, ImageAnalysis* analysis);
IplImage* cropByBoundary(IplImage* src, ImageAnalysis* analysis);
void visualizeBoundaryBox(IplImage* src, ImageAnalysis* analysis);
void visualizeAllEdges(IplImage* src, ImageAnalysis* analysis);

/*
	allocateAnalysis(int sizeX, int sizeY)
	목적: 이미지 분석을 위한 ImageAnalysis 구조체와 모든 배열의 메모리를 동적 할당
	동작:
		- ImageAnalysis 구조체 메모리 할당
		- 이미지 크기에 맞춰 다음 배열들을 할당:
		  * avgX, avgY: 각 열/행의 평균 밝기값 저장
		  * diffX, diffY: 1차 차분 (크기: sizeX-1, sizeY-1)
		  * diff2X, diff2Y: 2차 차분 (크기: sizeX-2, sizeY-2)
	반환: 할당된 ImageAnalysis 포인터
*/
ImageAnalysis* allocateAnalysis(int sizeX, int sizeY)
{
	ImageAnalysis* analysis = (ImageAnalysis*)malloc(sizeof(ImageAnalysis));
	analysis->avgX = (int*)malloc(sizeof(int) * sizeX);
	analysis->avgY = (int*)malloc(sizeof(int) * sizeY);
	analysis->diffX = (int*)malloc(sizeof(int) * (sizeX - 1));
	analysis->diffY = (int*)malloc(sizeof(int) * (sizeY - 1));
	analysis->diff2X = (int*)malloc(sizeof(int) * (sizeX - 2));
	analysis->diff2Y = (int*)malloc(sizeof(int) * (sizeY - 2));
	analysis->sizeX = sizeX;
	analysis->sizeY = sizeY;
	analysis->leftX = analysis->rightX = analysis->topY = analysis->bottomY = -1;
	return analysis;
}

/*
	freeAnalysis(ImageAnalysis* analysis)
	목적: allocateAnalysis()에서 할당된 모든 메모리를 해제
	동작:
		- ImageAnalysis 내부의 모든 배열 메모리 해제
		- ImageAnalysis 구조체 자신의 메모리 해제
		- 메모리 누수 방지
	입력: analysis - 해제할 ImageAnalysis 포인터
*/
void freeAnalysis(ImageAnalysis* analysis)
{
	if (analysis) {
		free(analysis->avgX);
		free(analysis->avgY);
		free(analysis->diffX);
		free(analysis->diffY);
		free(analysis->diff2X);
		free(analysis->diff2Y);
		free(analysis);
	}
}

/*
	showImageFit(const char* windowName, IplImage* img, int maxWidth, int maxHeight)
	목적: 이미지를 적절한 크기로 윈도우에 표시 (화면 크기를 초과하면 축소)
	동작:
		- 이미지를 maxWidth x maxHeight 내에 맞춰 표시
		- 이미지가 충분히 작으면 원래 크기로 표시
		- 이미지가 크면 비율을 유지하며 축소 표시
	입력:
		- windowName: 윈도우 제목
		- img: 표시할 이미지
		- maxWidth, maxHeight: 최대 표시 크기 (기본값: 1200x800)
*/
void showImageFit(const char* windowName, IplImage* img, int maxWidth = 1200, int maxHeight = 800)
{
	float scaleW = (float)maxWidth / img->width;
	float scaleH = (float)maxHeight / img->height;
	float scale = (scaleW < scaleH) ? scaleW : scaleH;

	if (scale < 1.0f) {
		IplImage* resized = cvCreateImage(cvSize((int)(img->width * scale), (int)(img->height * scale)),
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

/*
	brightnessAvgX(IplImage* img, int* avgX)
	목적: 이미지의 각 열(X축)에 대한 평균 밝기값 계산
	동작:
		- 이미지의 각 X 좌표(열)에서 모든 Y 좌표의 RGB 평균 밝기값을 계산
		- 계산된 값을 avgX 배열에 저장
		- 평균 밝기값을 그래프로 시각화하여 "X Brightness" 윈도우에 표시
		- Y축은 높이 정규화, X축은 열 위치 표현
	입력:
		- img: 분석할 이미지
		- avgX: 각 열의 평균 밝기값을 저장할 배열
	반환: 이미지 너비 (배열 크기)
*/
int brightnessAvgX(IplImage* img, int* avgX)
{
	IplImage* dst = cvCreateImage(cvSize(img->width, MAX_BRIGHTNESS), 8, RGB_CHANNELS);
	CvSize size = cvGetSize(img);

	for (int x = 0; x < size.width; x++) {
		int avg = 0;
		for (int y = 0; y < size.height; y++) {
			CvScalar c = cvGet2D(img, y, x);
			int sum = 0;
			for (int k = 0; k < RGB_CHANNELS; k++)
				sum += c.val[k];
			avg += sum / RGB_CHANNELS;
		}
		avg /= size.height;
		avgX[x] = avg;
		cvSet2D(dst, MAX_BRIGHTNESS - avg, x, cvScalar(255, 255, 255));
	}

	cvShowImage("X Brightness", dst);
	cvReleaseImage(&dst);
	return size.width;
}

/*
	brightnessAvgY(IplImage* img, int* avgY)
	목적: 이미지의 각 행(Y축)에 대한 평균 밝기값 계산
	동작:
		- 이미지의 각 Y 좌표(행)에서 모든 X 좌표의 RGB 평균 밝기값을 계산
		- 계산된 값을 avgY 배열에 저장
		- 평균 밝기값을 그래프로 시각화하여 "Y Brightness" 윈도우에 표시
		- Y축은 행 위치 표현, X축은 높이 정규화
	입력:
		- img: 분석할 이미지
		- avgY: 각 행의 평균 밝기값을 저장할 배열
	반환: 이미지 높이 (배열 크기)
*/
int brightnessAvgY(IplImage* img, int* avgY)
{
	IplImage* dst = cvCreateImage(cvSize(img->height, MAX_BRIGHTNESS), 8, RGB_CHANNELS);
	CvSize size = cvGetSize(img);

	for (int y = 0; y < size.height; y++) {
		int avg = 0;
		for (int x = 0; x < size.width; x++) {
			CvScalar c = cvGet2D(img, y, x);
			int sum = 0;
			for (int k = 0; k < RGB_CHANNELS; k++)
				sum += c.val[k];
			avg += sum / RGB_CHANNELS;
		}
		avg /= size.width;
		avgY[y] = avg;
		cvSet2D(dst, MAX_BRIGHTNESS - avg, y, cvScalar(255, 255, 255));
	}

	showImageFit("Y Brightness", dst);
	cvReleaseImage(&dst);
	return size.height;
}

/*
	drawLineByX(IplImage* img, int* pos, int size)
	목적: 지정된 X 좌표에 수직선(파란색)을 그려 이미지에 표시
	동작:
		- pos 배열에 저장된 각 X 좌표에서
		- 이미지의 위쪽부터 아래쪽까지 수직선 그리기 (파란색 255, 0, 0)
		- 이미지에 직접 그려지므로 이미지 내용이 수정됨
	입력:
		- img: 선을 그릴 이미지
		- pos: X 좌표 배열 (size 크기)
		- size: pos 배열의 길이
*/
void drawLineByX(IplImage* img, int* pos, int size)
{
	if (!img || !pos) return;

	for (int i = 0; i < size; i++) {
		if (pos[i] >= 0 && pos[i] < img->width) {
			for (int y = 0; y < img->height; y++) {
				cvSet2D(img, y, pos[i], cvScalar(255, 0, 0));
			}
		}
	}
}

/*
	drawLineByY(IplImage* img, int* pos, int size)
	목적: 지정된 Y 좌표에 수평선(녹색)을 그려 이미지에 표시
	동작:
		- pos 배열에 저장된 각 Y 좌표에서
		- 이미지의 왼쪽부터 오른쪽까지 수평선 그리기 (녹색 0, 255, 0)
		- 이미지에 직접 그려지므로 이미지 내용이 수정됨
	입력:
		- img: 선을 그릴 이미지
		- pos: Y 좌표 배열 (size 크기)
		- size: pos 배열의 길이
*/
void drawLineByY(IplImage* img, int* pos, int size)
{
	if (!img || !pos) return;

	for (int i = 0; i < size; i++) {
		if (pos[i] >= 0 && pos[i] < img->height) {
			for (int x = 0; x < img->width; x++) {
				cvSet2D(img, pos[i], x, cvScalar(0, 255, 0));
			}
		}
	}
}

/*
	findSecondDiff(int* diff, int* diff2, int size)
	목적: 1차 차분의 2차 차분을 계산하여 밝기 변화의 곡률(기울기 변화)을 분석
	동작:
		- diff2[i] = diff[i+1] - diff[i] (차분의 차분 = 곡률)
		- 2차 차분 값이 급격하게 변하는 피크 위치 감지 및 출력
		- 2차 차분 그래프를 시각화하여 "2nd Derivative" 윈도우에 표시
		- 임계값(±30) 범위를 빨간색 선으로 표시
	의미:
		- 2차 차분이 크면 밝기 변화가 급격하게 변함 (경계/엣지)
		- 2차 차분이 0에 가까우면 밝기가 점진적으로 변함 (평탄함)
	입력:
		- diff: 1차 차분 배열
		- diff2: 2차 차분을 저장할 배열
		- size: diff 배열의 크기
*/
void findSecondDiff(int* diff, int* diff2, int size)
{
	if (size < 2) return;

	// 2차 차분 계산: diff2[i] = diff[i+1] - diff[i]
	for (int i = 0; i < size - 1; i++)
		diff2[i] = diff[i + 1] - diff[i];

	IplImage* graph2 = cvCreateImage(cvSize(size, 600), 8, 3);
	cvSet(graph2, cvScalar(0, 0, 0));

	for (int x = 0; x < size - 2; x++)
	{
		if (abs(diff2[x] - diff2[x + 1]) > THRESHOLD)
			printf("2nd Derivative Peak at X: %d\n", x);

		int normalized = diff2[x] + 255;
		if (normalized >= 0 && normalized < 600) {
			cvSet2D(graph2, normalized, x, cvScalar(0, 255, 0));
		}

		cvSet2D(graph2, THRESHOLD + 255, x, cvScalar(0, 0, 255));
		cvSet2D(graph2, 255 - THRESHOLD, x, cvScalar(0, 0, 255));
	}

	showImageFit("2nd Derivative", graph2);
	cvReleaseImage(&graph2);
}

/*
	getDiffPos(int* diff, int pos[], int size)
	목적: 차분 배열에서 절댓값이 가장 큰 TOP_K(3)개의 위치를 찾아 저장
	동작:
		- diff 배열의 모든 요소를 순회하며 절댓값 계산
		- 절댓값이 가장 큰 3개를 크기 순으로 추적
		- 찾은 위치를 pos 배열에 저장 (큰 순서대로)
		- 찾지 못한 위치는 -1로 설정
	의미:
		- 밝기 변화(1차 차분)가 가장 급한 위치 3개를 검출
		- 엣지(경계) 검출의 핵심 단계
	입력:
		- diff: 1차 차분 배열
		- pos: 검출된 위치를 저장할 배열 (크기: TOP_K=3)
		- size: diff 배열의 크기
	반환: diff 포인터 (원본 배열 반환)
*/
int* getDiffPos(int* diff, int pos[], int size)
{
	// 초기화: pos 배열을 -1로 설정
	for (int i = 0; i < TOP_K; i++)
		pos[i] = -1;

	// size가 0 이하면 반환
	if (size <= 0) return diff;

	// TOP_K개의 최대 절댓값 위치를 저장하기 위한 변수
	int maxAbsValues[TOP_K];
	for (int i = 0; i < TOP_K; i++)
		maxAbsValues[i] = -1;

	// 배열을 순회하며 절댓값이 큰 TOP_K개 찾기
	for (int i = 0; i < size; i++) {
		int absVal = abs(diff[i]);

		// TOP_K개 중 가장 작은 값보다 현재 값이 크면 교체
		for (int k = 0; k < TOP_K; k++) {
			if (maxAbsValues[k] == -1 || absVal > abs(diff[maxAbsValues[k]])) {
				// 뒤로 한 칸씩 밀기
				for (int j = TOP_K - 1; j > k; j--) {
					maxAbsValues[j] = maxAbsValues[j - 1];
				}
				maxAbsValues[k] = i;
				break;
			}
		}
	}

	// 결과를 pos 배열에 복사
	for (int i = 0; i < TOP_K; i++) {
		pos[i] = maxAbsValues[i];
	}

	return diff;
}

/*
	findEdge(int* arr, int* diff, int size)
	목적: 평균 밝기값 배열의 1차 차분(엣지)을 계산
	동작:
		- arr 배열에서 인접한 요소의 차이를 계산 (diff[i] = arr[i+1] - arr[i])
		- 계산된 1차 차분을 그래프로 시각화하여 "diff" 윈도우에 표시
	의미:
		- 1차 차분이 크면 인접한 밝기값 사이의 차이가 큼 = 엣지(경계) 위치
		- 1차 차분이 작으면 밝기가 점진적으로 변함 = 평탄한 영역
	입력:
		- arr: 평균 밝기값 배열 (avgX 또는 avgY)
		- diff: 1차 차분을 저장할 배열
		- size: arr 배열의 크기
	반환: diff 포인터 (원본 배열 반환)
*/
int* findEdge(int* arr, int* diff, int size)
{
	for (int i = 1; i < size; i++)
		diff[i - 1] = arr[i] - arr[i - 1];
	IplImage* graph = cvCreateImage(cvSize(size, 600), 8, 3);

	showImageFit("diff", graph);
	cvReleaseImage(&graph);
	return diff;
}

/*
	findBoundary(int* diff, int size, int* leftOut, int* rightOut)
	목적: 1차 차분 배열에서 최대값과 최소값의 위치를 찾아 좌/우 경계 검출
	동작:
		- diff 배열을 전체 순회하며 최대값(양수 피크)과 최소값(음수 피크) 추적
		- 최소값(음수) 위치: 밝기가 감소하는 지점 = left(또는 top) 경계
		- 최대값(양수) 위치: 밝기가 증가하는 지점 = right(또는 bottom) 경계
	입력:
		- diff: 1차 차분 배열
		- size: diff 배열의 크기
		- leftOut: 좌측 경계 위치 저장 (최소값 위치)
		- rightOut: 우측 경계 위치 저장 (최대값 위치)
*/
void findBoundary(int* diff, int size, int* leftOut, int* rightOut)
{
	int maxPos = -1, maxVal = -1000;
	int minPos = -1, minVal = 1000;

	for (int i = 0; i < size; i++) {
		if (diff[i] > maxVal) {
			maxVal = diff[i];
			maxPos = i;
		}
		if (diff[i] < minVal) {
			minVal = diff[i];
			minPos = i;
		}
	}

	*leftOut = minPos;   // 음수 피크: 밝기 감소 지점 (왼쪽/위쪽 경계)
	*rightOut = maxPos;  // 양수 피크: 밝기 증가 지점 (오른쪽/아래쪽 경계)
}

/*
	processImageAnalysis(IplImage* img, ImageAnalysis* analysis)
	목적: 이미지의 전체 분석 프로세스 수행 (X축과 Y축 엣지 검출)
	동작:
		[X축(열) 분석]
		1. brightnessAvgX() - 각 열의 평균 밝기값 계산 및 시각화
		2. findEdge() - 1차 차분 계산 및 TOP_K 위치 검출 → posX 저장
		3. findSecondDiff() - 2차 차분 계산 및 시각화
		4. drawLineByX() - 검출된 X 위치에 수직선 그리기

		[Y축(행) 분석]
		5. brightnessAvgY() - 각 행의 평균 밝기값 계산 및 시각화
		6. findEdge() - 1차 차분 계산 및 TOP_K 위치 검출 → posY 저장
		7. findSecondDiff() - 2차 차분 계산 및 시각화
		(drawLineByY()는 visualizeResults()에서 호출)

	입력:
		- img: 분석할 이미지
		- analysis: 분석 결과를 저장할 ImageAnalysis 구조체 포인터
*/
void processImageAnalysis(IplImage* img, ImageAnalysis* analysis)
{
	brightnessAvgX(img, analysis->avgX);
	findEdge(analysis->avgX, analysis->diffX, analysis->sizeX);
	findBoundary(analysis->diffX, analysis->sizeX - 1, &analysis->leftX, &analysis->rightX);
	findSecondDiff(analysis->diffX, analysis->diff2X, analysis->sizeX - 1);

	brightnessAvgY(img, analysis->avgY);
	findEdge(analysis->avgY, analysis->diffY, analysis->sizeY);
	findBoundary(analysis->diffY, analysis->sizeY - 1, &analysis->topY, &analysis->bottomY);
	findSecondDiff(analysis->diffY, analysis->diff2Y, analysis->sizeY - 1);

	printf("Boundary: left=%d, right=%d, top=%d, bottom=%d\n",
		   analysis->leftX, analysis->rightX, analysis->topY, analysis->bottomY);
}

/*
	extractRGBChannels(IplImage* img, CvSize size, int gap, IplImage* dst)
	목적: 이미지를 3개의 동일한 높이의 영역으로 나누어 RGB 채널을 분리하고 재배열
	동작:
		- 원본 이미지를 3개 영역(높이 = size.height/3)으로 분할
		- 영역 1: Blue 채널 추출
		- 영역 2: Green 채널 추출
		- 영역 3: Red 채널 추출
		- 추출한 RGB를 합쳐서 목표 이미지에 저장 (색상 채널 재구성)
	입력:
		- img: RGB 채널이 혼합된 원본 이미지
		- size: 원본 이미지 크기
		- gap: 각 채널 영역의 높이 (보통 size.height / 3)
		- dst: 재배열된 RGB를 저장할 목표 이미지
*/
void extractRGBChannels(IplImage* img, CvSize size, int gap, IplImage* dst)
{
	for (int y = 0; y < gap; y++) {
		for (int x = 0; x < size.width; x++) {
			CvScalar blue = cvGet2D(img, y, x);
			CvScalar green = cvGet2D(img, y + gap, x);
			CvScalar red = cvGet2D(img, y + gap * 2, x);

			CvScalar out = cvScalar(red.val[0], green.val[1], blue.val[2]);
			cvSet2D(dst, y, x, out);
		}
	}
	cvShowImage("dst", dst);
}

/*
	cropByBoundary(IplImage* src, ImageAnalysis* analysis)
	목적: 검출된 경계 정보를 이용하여 ROI(관심 영역)를 추출
	동작:
		- analysis의 leftX, rightX, topY, bottomY를 이용한 ROI 설정
		- cvSetImageROI로 관심 영역 지정
		- cvCopy로 해당 영역 복사
		- 새로운 이미지로 반환
	입력:
		- src: 원본 이미지
		- analysis: 경계 정보를 담은 ImageAnalysis 구조체
	반환: 크롭된 이미지 (실패시 NULL)
*/
IplImage* cropByBoundary(IplImage* src, ImageAnalysis* analysis)
{
	int x1 = analysis->leftX,  x2 = analysis->rightX;
	int y1 = analysis->topY,   y2 = analysis->bottomY;

	if (x1 < 0 || x2 < 0 || y1 < 0 || y2 < 0) return NULL;
	if (x1 >= x2 || y1 >= y2) return NULL;

	cvSetImageROI(src, cvRect(x1, y1, x2 - x1, y2 - y1));
	IplImage* cropped = cvCreateImage(cvSize(x2 - x1, y2 - y1), src->depth, src->nChannels);
	cvCopy(src, cropped);
	cvResetImageROI(src);
	return cropped;
}

/*
	visualizeBoundaryBox(IplImage* src, ImageAnalysis* analysis)
	목적: 검출된 경계를 원본 이미지에 빨간 박스로 표시하여 시각화
	동작:
		- 원본 이미지의 복사본 생성
		- 검출된 경계 좌표로 빨간 사각형 박스 그리기
		- "Boundary Box" 윈도우에 표시
	입력:
		- src: 원본 이미지
		- analysis: 경계 정보를 담은 ImageAnalysis 구조체
*/
void visualizeBoundaryBox(IplImage* src, ImageAnalysis* analysis)
{
	if (!src || !analysis) return;

	int x1 = analysis->leftX,  x2 = analysis->rightX;
	int y1 = analysis->topY,   y2 = analysis->bottomY;

	if (x1 < 0 || x2 < 0 || y1 < 0 || y2 < 0) return;
	if (x1 >= x2 || y1 >= y2) return;

	IplImage* display = cvCreateImage(cvGetSize(src), 8, RGB_CHANNELS);
	cvCopy(src, display);

	cvRectangle(display, cvPoint(x1, y1), cvPoint(x2, y2),
				cvScalar(0, 0, 255), 2);  // 빨간색, 두께 2

	showImageFit("Boundary Box", display);
	cvReleaseImage(&display);
}

/*
	visualizeAllEdges(IplImage* src, ImageAnalysis* analysis)
	목적: 2차 차분의 피크(경계 변화)가 있는 모든 위치에 선을 그려 경계를 시각화
	동작:
		- diff2X에서 threshold 이상인 모든 피크 위치에 수직선 그리기 (초록색)
		- diff2Y에서 threshold 이상인 모든 피크 위치에 수평선 그리기 (초록색)
		- 이를 통해 밝기 변화의 변화(곡률)가 급격한 모든 지점 표시
	입력:
		- src: 원본 이미지
		- analysis: 2차 차분 정보를 담은 ImageAnalysis 구조체
*/
void visualizeAllEdges(IplImage* src, ImageAnalysis* analysis)
{
	if (!src || !analysis) return;

	IplImage* display = cvCreateImage(cvGetSize(src), 8, RGB_CHANNELS);
	cvCopy(src, display);

	// X축 경계 (수직선) - diff2X에서 피크가 나오는 모든 위치
	for (int x = 0; x < analysis->sizeX - 2; x++) {
		if (abs(analysis->diff2X[x] - analysis->diff2X[x + 1]) > THRESHOLD) {
			for (int y = 0; y < display->height; y++) {
				cvSet2D(display, y, x, cvScalar(0, 255, 0));  // 초록색
			}
		}
	}

	// Y축 경계 (수평선) - diff2Y에서 피크가 나오는 모든 위치
	for (int y = 0; y < analysis->sizeY - 2; y++) {
		if (abs(analysis->diff2Y[y] - analysis->diff2Y[y + 1]) > THRESHOLD) {
			for (int x = 0; x < display->width; x++) {
				cvSet2D(display, y, x, cvScalar(0, 255, 0));  // 초록색
			}
		}
	}

	showImageFit("All Edges", display);
	cvReleaseImage(&display);
}

/*
	visualizeResults(IplImage* img, ImageAnalysis* analysis)
	목적: 분석된 엣지 위치를 원본 이미지에 강조선으로 표시하여 최종 결과 시각화
	동작:
		- 원본 이미지의 복사본을 생성
		- analysis->posY[0~2]에서 검출된 Y 위치에 수평선 그리기 (회색)
		- analysis->posX[0~2]에서 검출된 X 위치에 수직선 그리기 (회색)
		- 그리드 형태의 강조선이 원본 이미지 위에 표시됨
		- "Result" 윈도우에 표시
	입력:
		- img: 원본 이미지
		- analysis: 엣지 위치 정보를 담은 ImageAnalysis 구조체
*/
//void visualizeResults(IplImage* img, ImageAnalysis* analysis)
//{
//	CvSize size = cvGetSize(img);
//	IplImage* showWhere = cvCreateImage(size, 8, RGB_CHANNELS);
//	cvCopy(img,showWhere);
//
//	// Draw horizontal lines for posY
//	for (int k = 0; k < TOP_K; k++) {
//		if (analysis->posY[k] >= 0 && analysis->posY[k] < size.height) {
//			for (int x = 0; x < size.width; x++) {
//				cvSet2D(showWhere, analysis->posY[k], x,
//					cvScalar(HIGHLIGHT_COLOR, HIGHLIGHT_COLOR, HIGHLIGHT_COLOR));
//			}
//		}
//	}
//
//	// Draw vertical lines for posX
//	for (int k = 0; k < TOP_K; k++) {
//		if (analysis->posX[k] >= 0 && analysis->posX[k] < size.width) {
//			for (int y = 0; y < size.height; y++) {
//				cvSet2D(showWhere, y, analysis->posX[k],
//					cvScalar(HIGHLIGHT_COLOR, HIGHLIGHT_COLOR, HIGHLIGHT_COLOR));
//			}
//		}
//	}
//
//	showImageFit("Result", showWhere);
//	cvReleaseImage(&showWhere);
//}

/*
	main()
	프로그램의 진입점 및 전체 이미지 처리 워크플로우 조정

	실행 순서:
	1. 이미지 로드 (c:\MultiMedia\AS2\pg1.jpg)
	2. ImageAnalysis 메모리 할당
	3. processImageAnalysis() - 완전한 이미지 분석
	   - X축, Y축 평균 밝기 계산
	   - 1차, 2차 차분 계산
	   - TOP_K 엣지 위치 검출
	4. extractRGBChannels() - RGB 채널 분리 및 재배열
	5. visualizeResults() - 엣지 위치에 강조선 표시
	6. 모든 결과 이미지 윈도우로 표시
	7. 키 입력 대기 (cvWaitKey)
	8. 모든 메모리 해제
	9. 프로그램 종료
*/
int main()
{
	IplImage* src1 = cvLoadImage("c:\\MultiMedia\\AS2\\pg1.jpg");

	if (!src1) {
		printf("Error: Cannot load image\n");
		return -1;
	}

	CvSize size = cvGetSize(src1);
	ImageAnalysis* analysis = allocateAnalysis(size.width, size.height);

	processImageAnalysis(src1, analysis);

	// 모든 경계 변화 표시
	visualizeAllEdges(src1, analysis);

	// 경계 박스 시각화
	visualizeBoundaryBox(src1, analysis);

	// 경계 기반으로 ROI 크롭
	IplImage* cropped = cropByBoundary(src1, analysis);

	if (cropped) {
		CvSize cropSize = cvGetSize(cropped);
		int gap = cropSize.height / 3;

		IplImage* dst = cvCreateImage(cvSize(cropSize.width, gap), 8, RGB_CHANNELS);
		extractRGBChannels(cropped, cropSize, gap, dst);
		cvReleaseImage(&dst);
		cvReleaseImage(&cropped);
	}

	cvWaitKey();

	freeAnalysis(analysis);
	cvReleaseImage(&src1);

	return 0;
}
