/*
	========================================
	Variance-Based RGB Channel Separation
	========================================

	분산(Variance)을 이용한 Y축 경계 검출
	- 각 행의 픽셀 밝기 변동성으로 edge 감지
	- RGB 영역은 높은 분산, 배경은 낮은 분산
	- 분산의 2차 차분으로 RGB 경계 위치 검출

	[PROGRAM FLOW]
	1. 이미지 로드
	2. Y축 분산 계산 (varY[y])
	3. 분산의 1차, 2차 차분
	4. 2차 차분에서 피크 검출 → Y1, Y2
	5. extractRGBChannels(0~Y1, Y1~Y2, Y2~height)
*/

#include<opencv2/opencv.hpp>
#include<stdlib.h>
#include<stdio.h>
#include<math.h>

// Constants
const int MAX_BRIGHTNESS = 255;
const int THRESHOLD = 30;  // 2차 차분 피크 검출 임계값
const int RGB_CHANNELS = 3;

// Variance Analysis structure
struct VarianceAnalysis {
	int* varY;		// Y축(행) 단위의 분산
	int* varX;		// X축(열) 단위의 분산
	int* diffVarY;	// Y축 분산의 1차 차분
	int* diffVarX;	// X축 분산의 1차 차분
	int* diff2VarY;	// Y축 분산의 2차 차분
	int* diff2VarX;	// X축 분산의 2차 차분
	int y1;			// Y축 첫 번째 경계 (RGB1과 RGB2 사이)
	int y2;			// Y축 두 번째 경계 (RGB2와 RGB3 사이)
	int sizeX;		// 이미지 너비
	int sizeY;		// 이미지 높이
};

// 함수 선언
VarianceAnalysis* allocateVarianceAnalysis(int sizeX, int sizeY);
void freeVarianceAnalysis(VarianceAnalysis* analysis);
void showImageFit(const char* windowName, IplImage* img, int maxWidth = 1200, int maxHeight = 800);
void calculateVarianceY(IplImage* img, VarianceAnalysis* analysis);
void calculateVarianceX(IplImage* img, VarianceAnalysis* analysis);
void findVarianceDiff(int* var, int* diffVar, int size);
void findVarianceSecondDiff(int* diffVar, int* diff2Var, int size);
void findVarianceYBoundaries(int* diff2VarY, int sizeY, int* y1, int* y2);
void visualizeVarianceY(IplImage* img, VarianceAnalysis* analysis);
void visualizeVarianceEdges(IplImage* src, VarianceAnalysis* analysis);
void extractRGBChannels(IplImage* img, CvSize size, int y1, int y2, IplImage* dst);

/*
	allocateVarianceAnalysis
	목적: VarianceAnalysis 구조체와 필요한 배열들을 동적 할당
*/
VarianceAnalysis* allocateVarianceAnalysis(int sizeX, int sizeY)
{
	VarianceAnalysis* analysis = (VarianceAnalysis*)malloc(sizeof(VarianceAnalysis));
	analysis->varY = (int*)malloc(sizeof(int) * sizeY);
	analysis->varX = (int*)malloc(sizeof(int) * sizeX);
	analysis->diffVarY = (int*)malloc(sizeof(int) * (sizeY - 1));
	analysis->diffVarX = (int*)malloc(sizeof(int) * (sizeX - 1));
	analysis->diff2VarY = (int*)malloc(sizeof(int) * (sizeY - 2));
	analysis->diff2VarX = (int*)malloc(sizeof(int) * (sizeX - 2));
	analysis->sizeX = sizeX;
	analysis->sizeY = sizeY;
	analysis->y1 = -1;
	analysis->y2 = -1;
	return analysis;
}

/*
	freeVarianceAnalysis
	목적: allocateVarianceAnalysis에서 할당된 메모리 해제
*/
void freeVarianceAnalysis(VarianceAnalysis* analysis)
{
	if (analysis) {
		free(analysis->varY);
		free(analysis->varX);
		free(analysis->diffVarY);
		free(analysis->diffVarX);
		free(analysis->diff2VarY);
		free(analysis->diff2VarX);
		free(analysis);
	}
}

/*
	showImageFit
	목적: 이미지를 적절한 크기로 윈도우에 표시
*/
void showImageFit(const char* windowName, IplImage* img, int maxWidth, int maxHeight)
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
	calculateVarianceY
	목적: 이미지의 각 행(Y)의 픽셀 밝기 분산을 계산
	동작:
		- 각 행 y에서:
		  1. 모든 x의 RGB 평균값 계산 → avgBrightness[x]
		  2. 평균값을 구한 뒤
		  3. 분산 = Σ(avgBrightness[x] - mean)² / width
		- 분산이 높으면 → RGB 콘텐츠 영역
		- 분산이 낮으면 → 균일한 배경
*/
void calculateVarianceY(IplImage* img, VarianceAnalysis* analysis)
{
	CvSize size = cvGetSize(img);

	// 각 행의 분산 계산
	for (int y = 0; y < size.height; y++) {
		// Step 1: 이 행의 모든 픽셀의 밝기값 계산
		int* rowBrightness = (int*)malloc(sizeof(int) * size.width);
		int sum = 0;

		for (int x = 0; x < size.width; x++) {
			CvScalar pixel = cvGet2D(img, y, x);
			int brightness = (pixel.val[0] + pixel.val[1] + pixel.val[2]) / RGB_CHANNELS;
			rowBrightness[x] = brightness;
			sum += brightness;
		}

		int mean = sum / size.width;

		// Step 2: 분산 계산
		int varianceSum = 0;
		for (int x = 0; x < size.width; x++) {
			int diff = rowBrightness[x] - mean;
			varianceSum += diff * diff;
		}

		analysis->varY[y] = varianceSum / size.width;  // 평균 제곱 편차

		free(rowBrightness);
	}

	// 분산 그래프 시각화
	IplImage* varGraph = cvCreateImage(cvSize(size.height, 600), 8, 3);
	cvSet(varGraph, cvScalar(0, 0, 0));

	for (int y = 0; y < size.height; y++) {
		int normalized = analysis->varY[y] / 5;  // 스케일 조정
		if (normalized >= 600) normalized = 599;

		cvSet2D(varGraph, 599 - normalized, y, cvScalar(255, 255, 255));
	}

	showImageFit("Variance Y", varGraph);
	cvReleaseImage(&varGraph);
}

/*
	calculateVarianceX
	목적: 이미지의 각 열(X)의 픽셀 밝기 분산을 계산
*/
void calculateVarianceX(IplImage* img, VarianceAnalysis* analysis)
{
	CvSize size = cvGetSize(img);

	for (int x = 0; x < size.width; x++) {
		int* colBrightness = (int*)malloc(sizeof(int) * size.height);
		int sum = 0;

		for (int y = 0; y < size.height; y++) {
			CvScalar pixel = cvGet2D(img, y, x);
			int brightness = (pixel.val[0] + pixel.val[1] + pixel.val[2]) / RGB_CHANNELS;
			colBrightness[y] = brightness;
			sum += brightness;
		}

		int mean = sum / size.height;

		int varianceSum = 0;
		for (int y = 0; y < size.height; y++) {
			int diff = colBrightness[y] - mean;
			varianceSum += diff * diff;
		}

		analysis->varX[x] = varianceSum / size.height;

		free(colBrightness);
	}

	// 분산 그래프 시각화
	IplImage* varGraph = cvCreateImage(cvSize(size.width, 600), 8, 3);
	cvSet(varGraph, cvScalar(0, 0, 0));

	for (int x = 0; x < size.width; x++) {
		int normalized = analysis->varX[x] / 5;
		if (normalized >= 600) normalized = 599;

		cvSet2D(varGraph, 599 - normalized, x, cvScalar(0, 255, 0));
	}

	showImageFit("Variance X", varGraph);
	cvReleaseImage(&varGraph);
}

/*
	findVarianceDiff
	목적: 분산 배열의 1차 차분 계산
	동작:
		- diffVar[i] = var[i+1] - var[i]
		- 분산의 변화를 나타냄
*/
void findVarianceDiff(int* var, int* diffVar, int size)
{
	for (int i = 1; i < size; i++)
		diffVar[i - 1] = var[i] - var[i - 1];
}

/*
	findVarianceSecondDiff
	목적: 분산의 1차 차분에 대한 2차 차분 계산
	동작:
		- diff2Var[i] = diffVar[i+1] - diffVar[i]
		- 분산 변화의 가속도(곡률) 나타냄
		- 이 값이 크면 → 급격한 경계 변화
*/
void findVarianceSecondDiff(int* diffVar, int* diff2Var, int size)
{
	for (int i = 0; i < size - 1; i++)
		diff2Var[i] = diffVar[i + 1] - diffVar[i];

	// 2차 차분 그래프
	IplImage* graph2 = cvCreateImage(cvSize(size, 600), 8, 3);
	cvSet(graph2, cvScalar(0, 0, 0));

	for (int x = 0; x < size - 1; x++) {
		int normalized = diff2Var[x] / 2 + 300;
		if (normalized >= 0 && normalized < 600) {
			cvSet2D(graph2, normalized, x, cvScalar(0, 255, 0));
		}

		cvSet2D(graph2, THRESHOLD + 300, x, cvScalar(0, 0, 255));
		cvSet2D(graph2, 300 - THRESHOLD, x, cvScalar(0, 0, 255));
	}

	showImageFit("2nd Variance Diff", graph2);
	cvReleaseImage(&graph2);
}

/*
	findVarianceYBoundaries
	목적: Y축 분산의 2차 차분에서 2개의 가장 큰 피크를 찾아 Y1, Y2 경계 검출
	동작:
		- threshold 이상의 피크를 모두 찾기
		- 가장 큰 2개만 선택
		- y1 < y2 보장
*/
void findVarianceYBoundaries(int* diff2VarY, int sizeY, int* y1, int* y2)
{
	*y1 = -1;
	*y2 = -1;

	int maxVal1 = -1000, maxPos1 = -1;  // 가장 큰 피크
	int maxVal2 = -1000, maxPos2 = -1;  // 두 번째 큰 피크

	for (int y = 0; y < sizeY - 1; y++) {
		int val = abs(diff2VarY[y] - diff2VarY[y + 1]);

		if (val > THRESHOLD) {
			if (val > maxVal1) {
				maxVal2 = maxVal1;
				maxPos2 = maxPos1;
				maxVal1 = val;
				maxPos1 = y;
			} else if (val > maxVal2) {
				maxVal2 = val;
				maxPos2 = y;
			}
		}
	}

	// 위치 정렬 (y1 < y2)
	if (maxPos1 >= 0 && maxPos2 >= 0) {
		*y1 = (maxPos1 < maxPos2) ? maxPos1 : maxPos2;
		*y2 = (maxPos1 < maxPos2) ? maxPos2 : maxPos1;
	} else if (maxPos1 >= 0) {
		*y1 = maxPos1;
	}

	printf("Variance Y Boundaries: y1=%d, y2=%d\n", *y1, *y2);
}

/*
	visualizeVarianceY
	목적: Y축 분산과 경계선을 시각화
*/
void visualizeVarianceY(IplImage* img, VarianceAnalysis* analysis)
{
	IplImage* display = cvCreateImage(cvGetSize(img), 8, 3);
	cvCopy(img, display);

	if (analysis->y1 >= 0) {
		for (int x = 0; x < display->width; x++) {
			cvSet2D(display, analysis->y1, x, cvScalar(0, 255, 0));  // 초록선
		}
	}

	if (analysis->y2 >= 0) {
		for (int x = 0; x < display->width; x++) {
			cvSet2D(display, analysis->y2, x, cvScalar(0, 255, 0));  // 초록선
		}
	}

	showImageFit("Variance Y Boundaries", display);
	cvReleaseImage(&display);
}

/*
	visualizeVarianceEdges
	목적: 분산 2차 차분에서 threshold 이상의 모든 피크에 선 그리기
*/
void visualizeVarianceEdges(IplImage* src, VarianceAnalysis* analysis)
{
	if (!src || !analysis) return;

	IplImage* display = cvCreateImage(cvGetSize(src), 8, 3);
	cvCopy(src, display);

	// Y축 모든 경계 (수평선)
	for (int y = 0; y < analysis->sizeY - 3; y++) {
		if (abs(analysis->diff2VarY[y] - analysis->diff2VarY[y + 1]) > THRESHOLD) {
			for (int x = 0; x < display->width; x++) {
				cvSet2D(display, y, x, cvScalar(255, 0, 0));  // 빨간색
			}
		}
	}

	// X축 모든 경계 (수직선)
	for (int x = 0; x < analysis->sizeX - 3; x++) {
		if (abs(analysis->diff2VarX[x] - analysis->diff2VarX[x + 1]) > THRESHOLD) {
			for (int y = 0; y < display->height; y++) {
				cvSet2D(display, y, x, cvScalar(255, 0, 0));  // 빨간색
			}
		}
	}

	showImageFit("All Variance Edges", display);
	cvReleaseImage(&display);
}

/*
	extractRGBChannels
	목적: Y1, Y2를 기준으로 이미지를 3개 piece로 나누고 RGB 채널 분리
	동작:
		- piece1: 0 ~ Y1 (height1)
		- piece2: Y1 ~ Y2 (height2)
		- piece3: Y2 ~ height (height3)
		- 3개 piece를 이어붙여 dst 이미지 생성
*/
void extractRGBChannels(IplImage* img, CvSize size, int y1, int y2, IplImage* dst)
{
	if (y1 < 0 || y2 < 0 || y1 >= y2 || y2 >= size.height) {
		printf("Invalid boundaries: y1=%d, y2=%d\n", y1, y2);
		return;
	}

	int h1 = y1;
	int h2 = y2 - y1;
	int h3 = size.height - y2;
	int gap = h1;  // 가장 작은 높이를 기준

	if (h2 < gap) gap = h2;
	if (h3 < gap) gap = h3;

	printf("RGB pieces: h1=%d, h2=%d, h3=%d, gap=%d\n", h1, h2, h3, gap);

	// 3개 piece에서 동일한 높이(gap)만큼만 추출
	for (int y = 0; y < gap; y++) {
		for (int x = 0; x < size.width; x++) {
			// Piece 1 (0 ~ Y1)
			CvScalar p1 = cvGet2D(img, y, x);
			// Piece 2 (Y1 ~ Y2)
			CvScalar p2 = cvGet2D(img, y1 + y, x);
			// Piece 3 (Y2 ~ height)
			CvScalar p3 = cvGet2D(img, y2 + y, x);

			// RGB로 재구성
			CvScalar out = cvScalar(p3.val[0], p2.val[1], p1.val[2]);
			cvSet2D(dst, y, x, out);
		}
	}
}

/*
	main
*/
int main()
{
	IplImage* src = cvLoadImage("c:\\MultiMedia\\AS2\\pg1.jpg");

	if (!src) {
		printf("Error: Cannot load image\n");
		return -1;
	}

	CvSize size = cvGetSize(src);
	VarianceAnalysis* analysis = allocateVarianceAnalysis(size.width, size.height);

	printf("=== Variance-Based Edge Detection ===\n");
	printf("Image size: %d x %d\n", size.width, size.height);

	// Y축 분석 (RGB 분리용)
	printf("\n[Y-Axis Analysis]\n");
	calculateVarianceY(src, analysis);
	findVarianceDiff(analysis->varY, analysis->diffVarY, analysis->sizeY);
	findVarianceSecondDiff(analysis->diffVarY, analysis->diff2VarY, analysis->sizeY - 1);
	findVarianceYBoundaries(analysis->diff2VarY, analysis->sizeY - 2, &analysis->y1, &analysis->y2);
	visualizeVarianceY(src, analysis);

	// X축 분석 (정보용)
	printf("\n[X-Axis Analysis]\n");
	calculateVarianceX(src, analysis);
	findVarianceDiff(analysis->varX, analysis->diffVarX, analysis->sizeX);
	findVarianceSecondDiff(analysis->diffVarX, analysis->diff2VarX, analysis->sizeX - 1);

	// 모든 edge 시각화
	visualizeVarianceEdges(src, analysis);

	// RGB 채널 분리 (Y1, Y2 기반)
	if (analysis->y1 >= 0 && analysis->y2 >= 0) {
		int gap = analysis->y1;
		if (analysis->y2 - analysis->y1 < gap) gap = analysis->y2 - analysis->y1;
		if (size.height - analysis->y2 < gap) gap = size.height - analysis->y2;

		IplImage* dst = cvCreateImage(cvSize(size.width, gap), 8, 3);
		extractRGBChannels(src, size, analysis->y1, analysis->y2, dst);
		showImageFit("RGB Extracted (Variance)", dst);
		cvReleaseImage(&dst);
	}

	cvWaitKey();

	freeVarianceAnalysis(analysis);
	cvReleaseImage(&src);

	return 0;
}
