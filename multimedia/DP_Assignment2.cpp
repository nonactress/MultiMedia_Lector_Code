#include<opencv2/opencv.hpp>
#include<stdlib.h>
#include<stdio.h>

// Constants
const int TOP_K = 3;
const int MAX_BRIGHTNESS = 255;
const int HIGHLIGHT_COLOR = 100;
const int RGB_CHANNELS = 3;

// Image Analysis structure
struct ImageAnalysis {
	int* avgX;
	int* avgY;
	int* diffX;
	int* diffY;
	int posX[TOP_K];
	int posY[TOP_K];
	int sizeX;
	int sizeY;
};

int brightnessAvgX(IplImage* img, int* avgX);
int brightnessAvgY(IplImage* img, int* avgY);
int* findEdge(int* arr, int* diff, int size, int pos[]);
int* getDiffpos(int* diff, int pos[], int size);
void swap(int* a, int* b);
int partition(int arr[], int low, int high);
void quickSort(int arr[], int low, int high);
void showImageFit(const char* windowName, IplImage* img, int maxWidth, int maxHeight);
ImageAnalysis* allocateAnalysis(int sizeX, int sizeY);
void freeAnalysis(ImageAnalysis* analysis);
void processImageAnalysis(IplImage* img, ImageAnalysis* analysis);
void extractRGBChannels(IplImage* img, CvSize size, int gap, IplImage* dst);
void visualizeResults(IplImage* img, ImageAnalysis* analysis);

ImageAnalysis* allocateAnalysis(int sizeX, int sizeY)
{
	ImageAnalysis* analysis = (ImageAnalysis*)malloc(sizeof(ImageAnalysis));
	analysis->avgX = (int*)malloc(sizeof(int) * sizeX);
	analysis->avgY = (int*)malloc(sizeof(int) * sizeY);
	analysis->diffX = (int*)malloc(sizeof(int) * (sizeX - 1));
	analysis->diffY = (int*)malloc(sizeof(int) * (sizeY - 1));
	analysis->sizeX = sizeX;
	analysis->sizeY = sizeY;
	return analysis;
}

void freeAnalysis(ImageAnalysis* analysis)
{
	if (analysis) {
		free(analysis->avgX);
		free(analysis->avgY);
		free(analysis->diffX);
		free(analysis->diffY);
		free(analysis);
	}
}

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

	cvShowImage("Y Brightness", dst);
	cvReleaseImage(&dst);
	return size.height;
}

void swap(int* a, int* b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

int partition(int arr[], int low, int high)
{
	int pivot = arr[high];
	int i = low - 1;

	for (int j = low; j < high; j++) {
		if (arr[j] < pivot) {
			i++;
			swap(&arr[i], &arr[j]);
		}
	}
	swap(&arr[i + 1], &arr[high]);
	return i + 1;
}

void quickSort(int arr[], int low, int high)
{
	if (low < high) {
		int pi = partition(arr, low, high);
		quickSort(arr, low, pi - 1);
		quickSort(arr, pi + 1, high);
	}
}

int* getDiffpos(int* diff, int pos[], int size)
{
	pos[0] = pos[1] = pos[2] = -1;

	if (size <= 0) return pos;

	int max1 = -999999, max2 = -999999, max3 = -999999;
	int idx1 = -1, idx2 = -1, idx3 = -1;

	for (int i = 0; i < size; i++) {
		if (diff[i] > max1) {
			max3 = max2;
			idx3 = idx2;
			max2 = max1;
			idx2 = idx1;
			max1 = diff[i];
			idx1 = i;
		} else if (diff[i] > max2) {
			max3 = max2;
			idx3 = idx2;
			max2 = diff[i];
			idx2 = i;
		} else if (diff[i] > max3) {
			max3 = diff[i];
			idx3 = i;
		}
	}

	pos[0] = idx1;
	pos[1] = idx2;
	pos[2] = idx3;

	return pos;
}

int* findEdge(int* arr, int* diff, int size, int pos[])
{
	for (int i = 1; i < size; i++)
		diff[i - 1] = arr[i] - arr[i - 1];

	return getDiffpos(diff, pos, size - 1);
}

void processImageAnalysis(IplImage* img, ImageAnalysis* analysis)
{
	brightnessAvgX(img, analysis->avgX);
	findEdge(analysis->avgX, analysis->diffX, analysis->sizeX, analysis->posX);

	brightnessAvgY(img, analysis->avgY);
	findEdge(analysis->avgY, analysis->diffY, analysis->sizeY, analysis->posY);
}

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
}

void visualizeResults(IplImage* img, ImageAnalysis* analysis)
{
	CvSize size = cvGetSize(img);
	IplImage* showWhere = cvCreateImage(size, 8, RGB_CHANNELS);
	cvZero(showWhere);

	// Draw horizontal lines for posY
	for (int k = 0; k < TOP_K; k++) {
		if (analysis->posY[k] >= 0 && analysis->posY[k] < size.height) {
			for (int x = 0; x < size.width; x++) {
				cvSet2D(showWhere, analysis->posY[k], x,
					cvScalar(HIGHLIGHT_COLOR, HIGHLIGHT_COLOR, HIGHLIGHT_COLOR));
			}
		}
	}

	// Draw vertical lines for posX
	for (int k = 0; k < TOP_K; k++) {
		if (analysis->posX[k] >= 0 && analysis->posX[k] < size.width) {
			for (int y = 0; y < size.height; y++) {
				cvSet2D(showWhere, y, analysis->posX[k],
					cvScalar(HIGHLIGHT_COLOR, HIGHLIGHT_COLOR, HIGHLIGHT_COLOR));
			}
		}
	}

	showImageFit("Result", showWhere);
	cvReleaseImage(&showWhere);
}

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

	CvSize dstSize = cvSize(size.width, size.height / 3);
	IplImage* dst = cvCreateImage(dstSize, 8, RGB_CHANNELS);
	extractRGBChannels(src1, size, size.height / 3, dst);

	visualizeResults(src1, analysis);

	cvShowImage("RGB Extracted", dst);
	showImageFit("Original", src1);

	cvWaitKey();

	cvReleaseImage(&dst);
	cvReleaseImage(&src1);
	freeAnalysis(analysis);

	return 0;
}
