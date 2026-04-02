#include <opencv2/opencv.hpp>

double calculateSSD(IplImage* base, IplImage* target, int dx, int dy) {
	int width = base->width;
	int height = base->height;

	int startX = width / 4, endX = width * 3 / 4;
	int startY = height / 4, endY = height * 3 / 4;

	double sum = 0.0f;
	int count = 0;

	for (int y = startY; y < endY; y++) {
		for (int x = startX; x < endX; x++) {
			int nx = x + dx;
			int ny = y + dy;

			if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

			//printf("calculateSsd: (%d, %d), (%d, %d)\n", x, y, nx, ny);

			CvScalar c = cvGet2D(base, y, x);
			CvScalar g = cvGet2D(target, ny, nx);

			double diff = c.val[0] - g.val[0];

			sum += (diff * diff);
			count += 1;
		}
	}

	return sum / count;
}

IplImage* alignImage(IplImage* base, IplImage* target) {
	int SEARCH_RANGE_X = 15, SEARCH_RANGE_Y = 35;
	int alignX = 0, alignY = 0;
	double minSsd = DBL_MAX;

	printf("alignY\n");
	for (int y = -SEARCH_RANGE_Y; y <= SEARCH_RANGE_Y; y++) {
		double ssd = calculateSSD(base, target, alignX, y);

		if (ssd < minSsd) {
			minSsd = ssd;
			alignY = y;
		}
	}

	minSsd = DBL_MAX;

	printf("alignX\n");
	for (int x = -SEARCH_RANGE_X; x <= SEARCH_RANGE_X; x++) {
		double ssd = calculateSSD(base, target, x, alignY);

		if (ssd < minSsd) {
			minSsd = ssd;
			alignX = x;
		}
	}

	IplImage* alignedImage = cvCreateImage(cvGetSize(base), 8, 1);

	for (int y = 0; y < alignedImage->height; y++) {
		for (int x = 0; x < alignedImage->width; x++) {
			int nx = x + alignX;
			int ny = y + alignY;

			if (nx < 0 || nx >= base->width || ny < 0 || ny >= base->height) continue;

			CvScalar c = cvGet2D(target, ny, nx);

			cvSet2D(alignedImage, y, x, c);
		}
	}

	return alignedImage;
}

int main() {
	IplImage* src = cvLoadImage("C:\\MultiMedia\\AS2\\pg1.jpg");
	int width = src->width, height = src->height / 3;

	IplImage* dest = cvCreateImage(cvSize(width, height), 8, 3);
	IplImage* blueChannel = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* greenChannel = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* redChannel = cvCreateImage(cvSize(width, height), 8, 1);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			cvSet2D(blueChannel, y, x, cvGet2D(src, y, x));
			cvSet2D(greenChannel, y, x, cvGet2D(src, y + height, x));
			cvSet2D(redChannel, y, x, cvGet2D(src, y + height * 2, x));
		}
	}

	printf("alignGreen\n");
	IplImage* alignedGreen = alignImage(blueChannel, greenChannel);
	printf("alignRed\n");
	IplImage* alignedRed = alignImage(blueChannel, redChannel);

	printf("set dest\n");
	for (int y = 0; y < dest->height; y++) {
		for (int x = 0; x < dest->width; x++) {
			CvScalar b = cvGet2D(blueChannel, y, x);
			CvScalar g = cvGet2D(alignedGreen, y, x);
			CvScalar r = cvGet2D(alignedRed, y, x);

			cvSet2D(dest, y, x, cvScalar(b.val[0], g.val[0], r.val[0]));
		}
	}

	cvShowImage("src", src);
	cvShowImage("dest", dest);

	cvWaitKey();

	return 0;
}