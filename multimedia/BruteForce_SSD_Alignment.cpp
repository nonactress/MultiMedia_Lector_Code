#include <opencv2/opencv.hpp>
#include <cstring>
#include <ctime>
#include <stdio.h>

typedef struct
{
	int xOffset;
	int yOffset;
} OptimalOffset;

OptimalOffset alignB(IplImage* greenChannel, IplImage* blueChannel);
OptimalOffset alignR(IplImage* greenChannel, IplImage* redChannel);

double calculateSSD_BruteForce(IplImage* base, IplImage* target, int dx, int dy) {
	int width = base->width;
	int height = base->height;

	long long sum = 0LL;
	int count = 0;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int tx = x + dx;
			int ty = y + dy;

			// Bounds checking to prevent overflow/underflow
			if (tx < 0 || tx >= width || ty < 0 || ty >= height) {
				continue;  // Skip out-of-bounds pixels
			}

			CvScalar baseScalar = cvGet2D(base, y, x);
			CvScalar targetScalar = cvGet2D(target, ty, tx);

			int base_val = (int)baseScalar.val[0];
			int target_val = (int)targetScalar.val[0];
			int diff = base_val - target_val;

			sum += (long long)diff * diff;
			count += 1;
		}
	}

	if (count > 0) {
		return (double)sum / count;
	}
	return DBL_MAX;
}

OptimalOffset alignB(IplImage* greenChannel, IplImage* blueChannel)
{
	int SEARCH_RANGE_X = 15, SEARCH_RANGE_Y = 35;
	int alignX = 0, alignY = 0;
	double minSsd = DBL_MAX;

	printf("alignB: Brute-force search in progress...\n");
	printf("  dx range: [-%d, +%d], dy range: [-%d, +%d]\n",
		SEARCH_RANGE_X, SEARCH_RANGE_X, SEARCH_RANGE_Y, SEARCH_RANGE_Y);
	printf("  Total combinations: %d x %d = %d iterations\n",
		(SEARCH_RANGE_X * 2 + 1), (SEARCH_RANGE_Y * 2 + 1),
		(SEARCH_RANGE_X * 2 + 1) * (SEARCH_RANGE_Y * 2 + 1));

	for (int dy = -SEARCH_RANGE_Y; dy <= SEARCH_RANGE_Y; dy++) {
		for (int dx = -SEARCH_RANGE_X; dx <= SEARCH_RANGE_X; dx++) {
			double ssd = calculateSSD_BruteForce(greenChannel, blueChannel, dx, dy);

			if (ssd < minSsd) {
				minSsd = ssd;
				alignX = dx;
				alignY = dy;
			}
		}
	}

	printf("alignB: Optimal offset found = (dx=%d, dy=%d), minSSD=%.2f\n", alignX, alignY, minSsd);
	OptimalOffset result;
	result.xOffset = alignX;
	result.yOffset = alignY;
	return result;
}

OptimalOffset alignR(IplImage* greenChannel, IplImage* redChannel)
{
	int SEARCH_RANGE_X = 15, SEARCH_RANGE_Y = 35;
	int alignX = 0, alignY = 0;
	double minSsd = DBL_MAX;

	printf("alignR: Brute-force search in progress...\n");
	printf("  dx range: [-%d, +%d], dy range: [-%d, +%d]\n",
		SEARCH_RANGE_X, SEARCH_RANGE_X, SEARCH_RANGE_Y, SEARCH_RANGE_Y);
	printf("  Total combinations: %d x %d = %d iterations\n",
		(SEARCH_RANGE_X * 2 + 1), (SEARCH_RANGE_Y * 2 + 1),
		(SEARCH_RANGE_X * 2 + 1) * (SEARCH_RANGE_Y * 2 + 1));

	for (int dy = -SEARCH_RANGE_Y; dy <= SEARCH_RANGE_Y; dy++) {
		for (int dx = -SEARCH_RANGE_X; dx <= SEARCH_RANGE_X; dx++) {
			double ssd = calculateSSD_BruteForce(greenChannel, redChannel, dx, dy);

			if (ssd < minSsd) {
				minSsd = ssd;
				alignX = dx;
				alignY = dy;
			}
		}
	}

	printf("alignR: Optimal offset found = (dx=%d, dy=%d), minSSD=%.2f\n", alignX, alignY, minSsd);
	OptimalOffset result;
	result.xOffset = alignX;
	result.yOffset = alignY;
	return result;
}


int main()
{
	printf("=== Brute-force SSD Channel Alignment ===\n");
	char imagePath[256];
	printf("Enter image path: ");
	fgets(imagePath, sizeof(imagePath), stdin);

	int len = strlen(imagePath);
	if (len > 0 && imagePath[len - 1] == '\n') {
		imagePath[len - 1] = '\0';
	}

	IplImage* src = cvLoadImage(imagePath);
	if (!src) {
		printf("Error: Could not load image: %s\n", imagePath);
		return -1;
	}

	printf("\nImage loaded successfully\n");
	printf("  Original size: %d x %d\n", src->width, src->height);

	int width = src->width;
	int height = src->height / 3;

	printf("  Each channel size: %d x %d\n", width, height);

	IplImage* dest = cvCreateImage(cvSize(width, height), 8, 3);
	IplImage* blueChannel = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* greenChannel = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* redChannel = cvCreateImage(cvSize(width, height), 8, 1);

	printf("\nSeparating channels...\n");

	const uchar* srcData = (const uchar*)src->imageData;
	int srcStep = src->widthStep;
	uchar* blueData = (uchar*)blueChannel->imageData;
	uchar* greenData = (uchar*)greenChannel->imageData;
	uchar* redData = (uchar*)redChannel->imageData;
	int dstStep = blueChannel->widthStep;

	for (int y = 0; y < height; y++) {
		const uchar* srcBlueRow = srcData + y * srcStep;
		const uchar* srcGreenRow = srcData + (y + height) * srcStep;
		const uchar* srcRedRow = srcData + (y + height * 2) * srcStep;

		uchar* dstBlueRow = blueData + y * dstStep;
		uchar* dstGreenRow = greenData + y * dstStep;
		uchar* dstRedRow = redData + y * dstStep;

		for (int x = 0; x < width; x++) {
			dstBlueRow[x] = srcBlueRow[x * 3 + 0];
			dstGreenRow[x] = srcGreenRow[x * 3 + 1];
			dstRedRow[x] = srcRedRow[x * 3 + 2];
		}
	}

	printf("Channel separation complete\n");

	printf("\n=== Starting Channel Alignment ===\n");

	time_t startTime = time(NULL);
	clock_t startClock = clock();

	printf("\nAligning Blue channel...\n");
	OptimalOffset offsetB = alignB(greenChannel, blueChannel);

	printf("\nAligning Red channel...\n");
	OptimalOffset offsetR = alignR(greenChannel, redChannel);

	clock_t endClock = clock();
	time_t endTime = time(NULL);

	double elapsedSeconds = (double)(endClock - startClock) / CLOCKS_PER_SEC;
	printf("\n=== Channel Alignment Complete ===\n");
	printf("Elapsed time: %.3f seconds\n", elapsedSeconds);

	printf("\nMerging channels...\n");

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
			uchar gVal = greenRow[x];

			int bx = x + offsetB.xOffset;
			int by = y + offsetB.yOffset;
			uchar bVal = 0;
			if (bx >= 0 && bx < width && by >= 0 && by < height) {
				bVal = blueDataM[by * blueStepM + bx];
			}

			int rx = x + offsetR.xOffset;
			int ry = y + offsetR.yOffset;
			uchar rVal = 0;
			if (rx >= 0 && rx < width && ry >= 0 && ry < height) {
				rVal = redDataM[ry * redStepM + rx];
			}

			destRow[x * 3 + 0] = bVal;
			destRow[x * 3 + 1] = gVal;
			destRow[x * 3 + 2] = rVal;
		}
	}

	printf("Channel merge complete\n");

	printf("\nDisplaying result image... (press any key to exit)\n");
	cvShowImage("Original", src);
	cvShowImage("Result (BruteForce)", dest);

	cvWaitKey();

	printf("\nReleasing memory...\n");
	cvReleaseImage(&src);
	cvReleaseImage(&dest);
	cvReleaseImage(&blueChannel);
	cvReleaseImage(&greenChannel);
	cvReleaseImage(&redChannel);

	printf("Done!\n");
	printf("\n=== Performance Summary ===\n");
	printf("Brute-force method:\n");
	printf("  Blue: dx=%d, dy=%d\n", offsetB.xOffset, offsetB.yOffset);
	printf("  Red:  dx=%d, dy=%d\n", offsetR.xOffset, offsetR.yOffset);
	printf("  Total execution time: %.3f seconds\n", elapsedSeconds);
	printf("\nCompare with howtofindSSD.cpp results to see the optimization effect.\n");

	return 0;
}
