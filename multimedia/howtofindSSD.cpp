#include<opencv2/opencv.hpp>
#include<ctime>
#include<cstring>
#include<windows.h>


typedef struct
{
	int xOffset;
	int yOffset;
}OptimalOffset;


OptimalOffset alignB(IplImage* greenChannel, IplImage* blueChannel);
OptimalOffset alignR(IplImage* greenChannel, IplImage* redChannel);

// Thread data structure for alignB
typedef struct {
	IplImage* greenChannel;
	IplImage* blueChannel;
	OptimalOffset result;
}AlignBThreadData;

// Thread data structure for alignR
typedef struct {
	IplImage* greenChannel;
	IplImage* redChannel;
	OptimalOffset result;
}AlignRThreadData;

// Thread function for alignB
DWORD WINAPI alignBThread(LPVOID param) {
	AlignBThreadData* data = (AlignBThreadData*)param;
	data->result = alignB(data->greenChannel, data->blueChannel);
	return 0;
}

// Thread function for alignR
DWORD WINAPI alignRThread(LPVOID param) {
	AlignRThreadData* data = (AlignRThreadData*)param;
	data->result = alignR(data->greenChannel, data->redChannel);
	return 0;
}

// Helper function: Calculate SSD (Sum of Squared Differences) between two images
// Optimized with direct pointer access (Method A) and integer arithmetic (Method C)
double calculateSSD(IplImage* base, IplImage* target, int dx, int dy) {
	int width = base->width;
	int height = base->height;

	int startX = width / 4, endX = width * 3 / 4;
	int startY = height / 4, endY = height * 3 / 4;

	// Boundary pre-check: if offset moves ROI outside image, skip entirely
	if ((startX + dx) < 0 || (endX - 1 + dx) >= width ||
		(startY + dy) < 0 || (endY - 1 + dy) >= height)
		return DBL_MAX;

	long long sum = 0LL;
	int count = 0;

	// Cast imageData to uchar* for direct pixel access
	const uchar* baseData = (const uchar*)base->imageData;
	const uchar* targetData = (const uchar*)target->imageData;
	int baseStep = base->widthStep;
	int targetStep = target->widthStep;

	for (int y = startY; y < endY; y++) {
		const uchar* baseRow = baseData + y * baseStep;
		const uchar* targetRow = targetData + (y + dy) * targetStep;

		for (int x = startX; x < endX; x++) {
			int base_val = baseRow[x];
			int target_val = targetRow[x + dx];
			int diff = base_val - target_val;

			sum += (long long)diff * diff;
			count += 1;
		}
	}

	return (double)sum / count;
}

// Align Blue channel to Green channel using SSD
OptimalOffset alignB(IplImage* greenChannel, IplImage* blueChannel)
{
	int SEARCH_RANGE_X = 15, SEARCH_RANGE_Y = 35;
	int alignX = 0, alignY = 0;
	double minSsd = DBL_MAX;

	// Step 1: Find best Y offset
	printf("alignB: searching Y offset...\n");
	for (int y = -SEARCH_RANGE_Y; y <= SEARCH_RANGE_Y; y++) {
		double ssd = calculateSSD(greenChannel, blueChannel, 0, y);

		if (ssd < minSsd) {
			minSsd = ssd;
			alignY = y;
		}
	}

	minSsd = DBL_MAX;

	// Step 2: Find best X offset
	printf("alignB: searching X offset...\n");
	for (int x = -SEARCH_RANGE_X; x <= SEARCH_RANGE_X; x++) {
		double ssd = calculateSSD(greenChannel, blueChannel, x, alignY);

		if (ssd < minSsd) {
			minSsd = ssd;
			alignX = x;
		}
	}

	printf("alignB: best offset = (dx=%d, dy=%d)\n", alignX, alignY);
	OptimalOffset result;
	result.xOffset = alignX;
	result.yOffset = alignY;
	return result;
}

// Align Red channel to Green channel using SSD
OptimalOffset alignR(IplImage* greenChannel, IplImage* redChannel)
{
	int SEARCH_RANGE_X = 15, SEARCH_RANGE_Y = 35;
	int alignX = 0, alignY = 0;
	double minSsd = DBL_MAX;

	// Step 1: Find best Y offset
	printf("alignR: searching Y offset...\n");
	for (int y = -SEARCH_RANGE_Y; y <= SEARCH_RANGE_Y; y++) {
		double ssd = calculateSSD(greenChannel, redChannel, 0, y);

		if (ssd < minSsd) {
			minSsd = ssd;
			alignY = y;
		}
	}

	minSsd = DBL_MAX;

	// Step 2: Find best X offset
	printf("alignR: searching X offset...\n");
	for (int x = -SEARCH_RANGE_X; x <= SEARCH_RANGE_X; x++) {
		double ssd = calculateSSD(greenChannel, redChannel, x, alignY);

		if (ssd < minSsd) {
			minSsd = ssd;
			alignX = x;
		}
	}

	printf("alignR: best offset = (dx=%d, dy=%d)\n", alignX, alignY);
	OptimalOffset result;
	result.xOffset = alignX;
	result.yOffset = alignY;
	return result;
}


int main()
{
	// Start timing
	clock_t totalStart = clock();

	printf("Test CV\n");
	char imagePath[256];
	printf("Enter image path: ");
	fgets(imagePath, sizeof(imagePath), stdin);

	// Remove trailing newline
	int len = strlen(imagePath);
	if (len > 0 && imagePath[len - 1] == '\n') {
		imagePath[len - 1] = '\0';
	}

	// Load image
	clock_t loadStart = clock();
	IplImage* src = cvLoadImage(imagePath);
	clock_t loadEnd = clock();
	double loadTime_ms = ((double)(loadEnd - loadStart) / CLOCKS_PER_SEC) * 1000;

	if (!src) {
		printf("Error: Cannot load image from path: %s\n", imagePath);
		return -1;
	}
	printf("Image load time: %.2f ms\n", loadTime_ms);

	int width = src->width;
	int height = src->height / 3;

	// Create channel images
	clock_t allocStart = clock();
	IplImage* dest = cvCreateImage(cvSize(width, height), 8, 3);
	IplImage* blueChannel = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* greenChannel = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* redChannel = cvCreateImage(cvSize(width, height), 8, 1);
	clock_t allocEnd = clock();
	double allocTime_ms = ((double)(allocEnd - allocStart) / CLOCKS_PER_SEC) * 1000;
	printf("Memory allocation time: %.2f ms\n", allocTime_ms);

	// Split channels from stacked image using direct pointer access (Method A)
	printf("Splitting channels...\n");
	clock_t splitStart = clock();

	const uchar* srcData = (const uchar*)src->imageData;
	int srcStep = src->widthStep;
	uchar* blueData = (uchar*)blueChannel->imageData;
	uchar* greenData = (uchar*)greenChannel->imageData;
	uchar* redData = (uchar*)redChannel->imageData;
	int dstStep = blueChannel->widthStep;

	// Split BGR channels from stacked image (3-byte channel extraction)
	for (int y = 0; y < height; y++) {
		const uchar* srcBlueRow = srcData + y * srcStep;
		const uchar* srcGreenRow = srcData + (y + height) * srcStep;
		const uchar* srcRedRow = srcData + (y + height * 2) * srcStep;

		uchar* dstBlueRow = blueData + y * dstStep;
		uchar* dstGreenRow = greenData + y * dstStep;
		uchar* dstRedRow = redData + y * dstStep;

		// Extract individual channels from BGR interleaved format
		for (int x = 0; x < width; x++) {
			dstBlueRow[x]  = srcBlueRow[x * 3 + 0];  // B channel
			dstGreenRow[x] = srcGreenRow[x * 3 + 1]; // G channel
			dstRedRow[x]   = srcRedRow[x * 3 + 2];   // R channel
		}
	}

	clock_t splitEnd = clock();
	double splitTime_ms = ((double)(splitEnd - splitStart) / CLOCKS_PER_SEC) * 1000;
	printf("Split time: %.2f ms\n", splitTime_ms);

	// Align Blue and Red channels to Green channel (Method B: Multi-threading with CreateThread)
	clock_t alignStart = clock();

	AlignBThreadData dataB;
	dataB.greenChannel = greenChannel;
	dataB.blueChannel = blueChannel;

	AlignRThreadData dataR;
	dataR.greenChannel = greenChannel;
	dataR.redChannel = redChannel;

	HANDLE hThreadB = CreateThread(NULL, 0, alignBThread, &dataB, 0, NULL);
	HANDLE hThreadR = CreateThread(NULL, 0, alignRThread, &dataR, 0, NULL);

	if (hThreadB == NULL || hThreadR == NULL) {
		printf("Error: Failed to create threads\n");
		return -1;
	}

	WaitForSingleObject(hThreadB, INFINITE);
	WaitForSingleObject(hThreadR, INFINITE);

	CloseHandle(hThreadB);
	CloseHandle(hThreadR);

	OptimalOffset offsetB = dataB.result;
	OptimalOffset offsetR = dataR.result;

	clock_t alignEnd = clock();
	double alignTime_ms = ((double)(alignEnd - alignStart) / CLOCKS_PER_SEC) * 1000;
	printf("Alignment time: %.2f ms\n", alignTime_ms);

	// Merge aligned channels into RGB image using direct pointer access (Method A)
	printf("Merging channels...\n");
	clock_t mergeStart = clock();

	const uchar* blueDataM = (const uchar*)blueChannel->imageData;
	const uchar* greenDataM = (const uchar*)greenChannel->imageData;
	const uchar* redDataM = (const uchar*)redChannel->imageData;
	uchar* destDataM = (uchar*)dest->imageData;

	int blueStepM = blueChannel->widthStep;
	int greenStepM = greenChannel->widthStep;
	int redStepM = redChannel->widthStep;
	int destStepM = dest->widthStep;

	for (int y = 0; y < dest->height; y++) {
		const uchar* blueRow = blueDataM + y * blueStepM;
		const uchar* greenRow = greenDataM + y * greenStepM;
		const uchar* redRow = redDataM + y * redStepM;
		uchar* destRow = destDataM + y * destStepM;

		for (int x = 0; x < dest->width; x++) {
			// Green (Reference, no offset)
			uchar gVal = greenRow[x];

			// Blue (Apply offset, check bounds)
			int bx = x + offsetB.xOffset;
			int by = y + offsetB.yOffset;
			uchar bVal = 0;
			if (bx >= 0 && bx < width && by >= 0 && by < height) {
				bVal = blueDataM[by * blueStepM + bx];
			}

			// Red (Apply offset, check bounds)
			int rx = x + offsetR.xOffset;
			int ry = y + offsetR.yOffset;
			uchar rVal = 0;
			if (rx >= 0 && rx < width && ry >= 0 && ry < height) {
				rVal = redDataM[ry * redStepM + rx];
			}

			// Set RGB values in destination (BGR order for OpenCV)
			destRow[x * 3 + 0] = bVal;  // B
			destRow[x * 3 + 1] = gVal;  // G
			destRow[x * 3 + 2] = rVal;  // R
		}
	}

	clock_t mergeEnd = clock();
	double mergeTime_ms = ((double)(mergeEnd - mergeStart) / CLOCKS_PER_SEC) * 1000;
	printf("Merge time: %.2f ms\n", mergeTime_ms);

	cvShowImage("Original", src);
	cvShowImage("Result", dest);

	// End timing and calculate elapsed time
	clock_t totalEnd = clock();
	double totalTime_ms = ((double)(totalEnd - totalStart) / CLOCKS_PER_SEC) * 1000;

	printf("\n");
	printf("================== Performance Summary ==================\n");
	printf("Image load time:       %.2f ms\n", loadTime_ms);
	printf("Memory allocation:     %.2f ms\n", allocTime_ms);
	printf("Channel split time:    %.2f ms\n", splitTime_ms);
	printf("Alignment time:        %.2f ms\n", alignTime_ms);
	printf("Merge time:            %.2f ms\n", mergeTime_ms);
	printf("========================================================\n");
	printf("Total time (input to display): %.2f ms\n", totalTime_ms);
	printf("========================================================\n");

	cvWaitKey();

	// Clean up
	cvReleaseImage(&src);
	cvReleaseImage(&dest);
	cvReleaseImage(&blueChannel);
	cvReleaseImage(&greenChannel);
	cvReleaseImage(&redChannel);

	return 0;
}