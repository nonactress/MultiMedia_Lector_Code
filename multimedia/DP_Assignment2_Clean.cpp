/*
========================================
Image Processing: Boundary Detection & ROI Extraction
========================================
Pipeline-based architecture for easy flow understanding

STRUCTURE:
[1] Headers & Constants
[2] Data Structures
[3] Memory Management (alloc/free)
[4] Processing Stages (각 단계별 독립함수)
[5] Visualization & Output
[6] Main Pipeline Orchestrator
[7] Entry Point (main)
========================================
*/

#include<opencv2/opencv.hpp>
#include<stdlib.h>
#include<stdio.h>
#include<math.h>

// ============================================
// [1] CONFIGURATION & CONSTANTS
// ============================================
// Image loading
const char* INPUT_IMAGE_PATH = "c:\\MultiMedia\\AS2\\pg1.jpg";

// Processing parameters
const int BRIGHTNESS_LEVELS = 256;
const int RGB_CHANNELS = 3;
const int WINDOW_MAX_WIDTH = 1200;
const int WINDOW_MAX_HEIGHT = 800;

// ============================================
// [2] DATA STRUCTURES
// ============================================
struct ImageAnalysis {
	// Brightness data (1D arrays)
	int* avgX;      // X-axis (column) average brightness
	int* avgY;      // Y-axis (row) average brightness

	int* stdX;
	int* stdY;

	// Derivative data (1D arrays)
	int* diffX;     // X-axis 1st derivative
	int* diffY;     // Y-axis 1st derivative

	// Standard deviation derivative
	int* stddevDiff;  // Standard deviation 1st derivative (size: sizeY-1)

	// Boundaries (12 total: Top 4 Red, Top 4 Green, Top 4 Blue)
	int boundaries[12];  // Top 12 boundary positions with largest absolute values

	// Image dimensions
	int sizeX;      // Image width
	int sizeY;      // Image height

	// Detected boundaries
	int leftX;      // Left boundary
	int rightX;     // Right boundary
	int topY;       // Top boundary
	int bottomY;    // Bottom boundary
};

// ============================================
// [3] MEMORY MANAGEMENT
// ============================================
ImageAnalysis* allocateAnalysis(int sizeX, int sizeY)
{
	ImageAnalysis* analysis = (ImageAnalysis*)malloc(sizeof(ImageAnalysis));
	analysis->avgX = (int*)malloc(sizeof(int) * sizeX);
	analysis->avgY = (int*)malloc(sizeof(int) * sizeY);
	analysis->stdX = (int*)malloc(sizeof(int) * sizeX);
	analysis->stdY = (int*)malloc(sizeof(int) * sizeY);
	analysis->diffX = (int*)malloc(sizeof(int) * (sizeX - 1));
	analysis->diffY = (int*)malloc(sizeof(int) * (sizeY - 1));
	analysis->stddevDiff = (int*)malloc(sizeof(int) * (sizeY - 1));
	analysis->sizeX = sizeX;
	analysis->sizeY = sizeY;
	analysis->leftX = analysis->rightX = analysis->topY = analysis->bottomY = -1;
	for (int i = 0; i < 12; i++) {
		analysis->boundaries[i] = -1;
	}
	return analysis;
}

void freeAnalysis(ImageAnalysis* analysis)
{
	if (analysis) {
		free(analysis->avgX);
		free(analysis->avgY);
		free(analysis->stdX);
		free(analysis->stdY);
		free(analysis->diffX);
		free(analysis->diffY);
		free(analysis->stddevDiff);
		free(analysis);
	}
}

// ============================================
// [4] PROCESSING STAGE FUNCTIONS
// ============================================
// === STAGE 1: BRIGHTNESS CALCULATION ===
void stage_calculateBrightnessX(IplImage* img, int* avgX)
{
	int sizeX = img->width;
	for (int x = 0; x < sizeX; x++) {
		int sum = 0;
		for (int y = 0; y < img->height; y++) {
			CvScalar pixel = cvGet2D(img, y, x);
			int brightness = (pixel.val[0] + pixel.val[1] + pixel.val[2]) / RGB_CHANNELS;
			sum += brightness;
		}
		avgX[x] = sum / img->height;
	}
	printf("[STAGE 1-X] Brightness calculated for X-axis (%d columns)\n", sizeX);
}

void stage_calculateBrightnessY(IplImage* img, int* avgY)
{
	int sizeY = img->height;
	for (int y = 0; y < sizeY; y++) {
		int sum = 0;
		for (int x = 0; x < img->width; x++) {
			CvScalar pixel = cvGet2D(img, y, x);
			int brightness = (pixel.val[0] + pixel.val[1] + pixel.val[2]) / RGB_CHANNELS;
			sum += brightness;
		}
		avgY[y] = sum / img->width;
	}
	printf("[STAGE 1-Y] Brightness calculated for Y-axis (%d rows)\n", sizeY);
}

// === STAGE 1.5: VARIANCE CALCULATION ===
void stage_calculateVarianceX(IplImage* img, int* avgX, int* stdX)
{
	int sizeX = img->width;
	for (int x = 0; x < sizeX; x++) {
		long long sumSq = 0;
		for (int y = 0; y < img->height; y++) {
			CvScalar pixel = cvGet2D(img, y, x);
			int brightness = (pixel.val[0] + pixel.val[1] + pixel.val[2]) / RGB_CHANNELS;
			int diff = brightness - avgX[x];
			sumSq += diff * diff;
		}
		stdX[x] = (int)sqrt(sumSq / img->height);
	}
	printf("[STAGE 1.5-X] Standard Deviation calculated for X-axis (%d columns)\n", sizeX);
}

void stage_calculateVarianceY(IplImage* img, int* avgY, int* stdY)
{
	int sizeY = img->height;
	for (int y = 0; y < sizeY; y++) {
		long long sumSq = 0;
		for (int x = 0; x < img->width; x++) {
			CvScalar pixel = cvGet2D(img, y, x);
			int brightness = (pixel.val[0] + pixel.val[1] + pixel.val[2]) / RGB_CHANNELS;
			int diff = brightness - avgY[y];
			sumSq += diff * diff;
		}
		stdY[y] = (int)sqrt(sumSq / img->width);
	}
	printf("[STAGE 1.5-Y] Standard Deviation calculated for Y-axis (%d rows)\n", sizeY);
}

// === STAGE 2: FIRST DERIVATIVE (Edge Detection) ===
// avgSize is size of avgX or avgY, diff size is avgSize-1
void stage_calculateFirstDerivative(int* avg, int* diff, int avgSize)
{
	for (int i = 0; i < avgSize - 1; i++) {
		diff[i] = avg[i + 1] - avg[i];
	}
}

// === STAGE 2.5: STANDARD DEVIATION DERIVATIVE ===
// Calculate 1st derivative of standard deviation to detect boundary changes
void stage_calculateStddevDiff(int* stdY, int* stddevDiff, int sizeY)
{
	for (int y = 0; y < sizeY - 1; y++) {
		stddevDiff[y] = stdY[y + 1] - stdY[y];
	}
	printf("[STAGE 2.5] Standard Deviation Diff calculated (%d changes)\n", sizeY - 1);
}

// === STAGE 3: FIND TOP 12 BOUNDARIES ===
// Find top 12 boundary positions with largest absolute values and sort by Y-axis
void stage_findTop12Boundaries(int* stddevDiff, int sizeY, int* boundaries)
{
	// Temporary arrays: store absolute values and positions
	int tempPositions[12];
	int tempAbsDiffs[12];
	for (int i = 0; i < 12; i++) {
		tempPositions[i] = -1;
		tempAbsDiffs[i] = -1;
	}

	// Step 1: Find top 12 positions with largest absolute values
	for (int y = 0; y < sizeY - 1; y++) {
		int absVal = abs(stddevDiff[y]);

		// Compare current value with top 12
		for (int i = 0; i < 12; i++) {
			if (absVal > tempAbsDiffs[i]) {
				// Shift existing values backward
				for (int j = 11; j > i; j--) {
					tempAbsDiffs[j] = tempAbsDiffs[j - 1];
					tempPositions[j] = tempPositions[j - 1];
				}
				// Insert new value
				tempAbsDiffs[i] = absVal;
				tempPositions[i] = y;
				break;
			}
		}
	}

	// Step 2: Sort by Y-axis order (bubble sort)
	for (int i = 0; i < 12; i++) {
		for (int j = 0; j < 11 - i; j++) {
			if (tempPositions[j] > tempPositions[j + 1]) {
				// Swap positions
				int tempPos = tempPositions[j];
				tempPositions[j] = tempPositions[j + 1];
				tempPositions[j + 1] = tempPos;

				// Swap absDiffs
				int tempAbsVal = tempAbsDiffs[j];
				tempAbsDiffs[j] = tempAbsDiffs[j + 1];
				tempAbsDiffs[j + 1] = tempAbsVal;
			}
		}
	}

	// Step 3: Copy to boundaries array
	for (int i = 0; i < 12; i++) {
		boundaries[i] = tempPositions[i];
	}

	// Step 4: Print results
	printf("[STAGE 3] Top 12 Boundaries found:\n");
	for (int i = 0; i < 12; i++) {
		printf("  Boundary[%d]: y=%d (absDiff=%d)\n", i, boundaries[i], tempAbsDiffs[i]);
	}
}

// === STAGE 4: BOUNDARY DETECTION ===
void stage_findBoundary(int* diff, int size, int* boundaryLeft, int* boundaryRight)
{
	*boundaryLeft = -1;
	*boundaryRight = -1;

	int minVal = 0, minPos = -1;
	int maxVal = 0, maxPos = -1;

	for (int i = 0; i < size; i++) {
		if (diff[i] < minVal) {
			minVal = diff[i];
			minPos = i;
		}
		if (diff[i] > maxVal) {
			maxVal = diff[i];
			maxPos = i;
		}
	}

	*boundaryLeft = minPos;
	*boundaryRight = maxPos;
	printf("[STAGE 4] Boundary detected: min=%d, max=%d\n", minPos, maxPos);
}

// ============================================
// [5] VISUALIZATION & OUTPUT FUNCTIONS
// ============================================
void showImageFit(const char* windowName, IplImage* img, int maxWidth, int maxHeight)
{
	if (!img) return;

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

void visualize_brightnessGraphX(ImageAnalysis* analysis)
{
	int maxX = analysis->sizeX;
	if (maxX > 1200) maxX = 1200;  // Limit graph width

	IplImage* graph = cvCreateImage(cvSize(maxX, 400), 8, 3);
	cvSet(graph, cvScalar(0, 0, 0));

	for (int x = 0; x < maxX && x < analysis->sizeX; x++) {
		int y = 400 - (analysis->avgX[x] / 2);
		if (y >= 0 && y < 400) {
			for (int dy = -2; dy <= 2; dy++) {
				if (y + dy >= 0 && y + dy < 400) {
					cvSet2D(graph, y + dy, x, cvScalar(255, 255, 255));
				}
			}
		}
	}

	showImageFit("Brightness X", graph, WINDOW_MAX_WIDTH, WINDOW_MAX_HEIGHT);
	cvReleaseImage(&graph);
}

void visualize_brightnessGraphY(ImageAnalysis* analysis)
{
	int maxY = analysis->sizeY;

	IplImage* graph = cvCreateImage(cvSize(800, analysis->sizeY), 8, 3);
	cvSet(graph, cvScalar(0, 0, 0));

	for (int y = 0; y < maxY && y < analysis->sizeY; y++) {
		int x = 400 - (analysis->avgY[y] / 2);
		if (x >= 0 && x < 800) {
			for (int dx = -2; dx <= 2; dx++) {
				if (x + dx >= 0 && x + dx < 800) {
					cvSet2D(graph, y, x + dx, cvScalar(255, 255, 255));
				}
			}
		}
	}

	showImageFit("Brightness Y", graph, WINDOW_MAX_WIDTH, WINDOW_MAX_HEIGHT);
	cvReleaseImage(&graph);
}

void visualize_stddevGraphY(ImageAnalysis* analysis)
{
	int maxY = analysis->sizeY;

	IplImage* graph = cvCreateImage(cvSize(800, analysis->sizeY), 8, 3);
	cvSet(graph, cvScalar(0, 0, 0));

	for (int y = 0; y < maxY && y < analysis->sizeY; y++) {
		int x = 400 - (analysis->stdY[y] / 2);
		if (x >= 0 && x < 800) {
			for (int dx = -2; dx <= 2; dx++) {
				if (x + dx >= 0 && x + dx < 800) {
					cvSet2D(graph, y, x + dx, cvScalar(0, 255, 255));
				}
			}
		}
	}

	showImageFit("Standard Deviation Y", graph, WINDOW_MAX_WIDTH, WINDOW_MAX_HEIGHT);
	cvReleaseImage(&graph);
}

void visualize_boundaryBox(IplImage* src, ImageAnalysis* analysis)
{
	if (!src || !analysis) return;

	IplImage* display = cvCreateImage(cvGetSize(src), 8, 3);
	cvCopy(src, display);

	int x1 = analysis->leftX, x2 = analysis->rightX;
	int y1 = analysis->topY, y2 = analysis->bottomY;

	if (x1 >= 0 && x2 >= 0 && y1 >= 0 && y2 >= 0) {
		cvRectangle(display, cvPoint(x1, y1), cvPoint(x2, y2), cvScalar(0, 255, 0), 2);
		printf("[VISUALIZE] Boundary box: (%d,%d) to (%d,%d)\n", x1, y1, x2, y2);
	}

	showImageFit("Boundary Box", display, WINDOW_MAX_WIDTH, WINDOW_MAX_HEIGHT);
	cvReleaseImage(&display);
}

void visualize_detected_boundaries(IplImage* src, ImageAnalysis* analysis)
{
	if (!src || !analysis) return;

	IplImage* display = cvCreateImage(cvGetSize(src), 8, 3);
	cvCopy(src, display);

	// Draw horizontal lines at each boundary position
	// boundaries[0-3]: Red, boundaries[4-7]: Green, boundaries[8-11]: Blue
	for (int i = 0; i < 12; i++) {
		if (analysis->boundaries[i] >= 0) {
			int y = analysis->boundaries[i];
			CvScalar color;

			// Determine color based on group
			if (i < 4) {
				color = cvScalar(0, 0, 255);      // Red
			} else if (i < 8) {
				color = cvScalar(0, 255, 0);      // Green
			} else {
				color = cvScalar(255, 0, 0);      // Blue
			}

			cvLine(display, cvPoint(0, y), cvPoint(src->width, y), color, 2);
			printf("[VISUALIZE] Boundary[%d]: y=%d (Color: %s)\n", i, y,
				i < 4 ? "Red" : (i < 8 ? "Green" : "Blue"));
		}
	}

	showImageFit("Detected Boundaries", display, WINDOW_MAX_WIDTH, WINDOW_MAX_HEIGHT);
	cvReleaseImage(&display);
}

// ============================================
// [6] MAIN PIPELINE ORCHESTRATOR
// ============================================
void pipeline_analyzeImage(IplImage* img, ImageAnalysis* analysis)
{
	printf("\n========== PIPELINE START ==========\n");

	// STAGE 1: Calculate brightness along both axes
	printf("\n[STAGE 1] Brightness Analysis\n");
	stage_calculateBrightnessX(img, analysis->avgX);
	stage_calculateBrightnessY(img, analysis->avgY);
	visualize_brightnessGraphX(analysis);
	visualize_brightnessGraphY(analysis);

	// STAGE 1.5: Calculate standard deviation
	printf("\n[STAGE 1.5] Standard Deviation Analysis\n");
	stage_calculateVarianceX(img, analysis->avgX, analysis->stdX);
	stage_calculateVarianceY(img, analysis->avgY, analysis->stdY);
	visualize_stddevGraphY(analysis);

	// STAGE 2.5: Calculate standard deviation derivative
	printf("\n[STAGE 2.5] Standard Deviation Derivative\n");
	stage_calculateStddevDiff(analysis->stdY, analysis->stddevDiff, analysis->sizeY);

	// STAGE 3: Find top 12 boundaries
	printf("\n[STAGE 3] Finding Top 12 Boundaries\n");
	stage_findTop12Boundaries(analysis->stddevDiff, analysis->sizeY, analysis->boundaries);

	// STAGE 2: First derivative (edge detection)
	printf("\n[STAGE 2] First Derivative (Edge Detection)\n");
	stage_calculateFirstDerivative(analysis->avgX, analysis->diffX, analysis->sizeX);
	stage_calculateFirstDerivative(analysis->avgY, analysis->diffY, analysis->sizeY);

	// STAGE 4: Boundary detection
	printf("\n[STAGE 4] Boundary Detection\n");
	stage_findBoundary(analysis->diffX, analysis->sizeX - 1, &analysis->leftX, &analysis->rightX);
	stage_findBoundary(analysis->diffY, analysis->sizeY - 1, &analysis->topY, &analysis->bottomY);

	printf("\n========== PIPELINE END ==========\n");
}

// ============================================
// [7] ENTRY POINT
// ============================================
int main()
{
	printf("=== Image Processing: Boundary Detection ===\n");

	// Load image
	IplImage* src = cvLoadImage(INPUT_IMAGE_PATH);
	if (!src) {
		printf("ERROR: Cannot load image from %s\n", INPUT_IMAGE_PATH);
		return -1;
	}

	printf("Image loaded: %d x %d\n", src->width, src->height);

	// Allocate analysis structure
	ImageAnalysis* analysis = allocateAnalysis(src->width, src->height);

	// Run analysis pipeline
	pipeline_analyzeImage(src, analysis);

	// Visualize results
	visualize_boundaryBox(src, analysis);
	visualize_detected_boundaries(src, analysis);

	// Wait for user input
	printf("\nPress any key to exit...\n");
	cvWaitKey();

	// Cleanup
	freeAnalysis(analysis);
	cvReleaseImage(&src);

	printf("Program ended successfully.\n");
	return 0;
}
