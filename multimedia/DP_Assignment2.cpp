#include<opencv2/opencv.hpp>
void brightnessAvgX(IplImage* img, const char* name, int* avgX);
void brightnessAvgY(IplImage* img, const char* name, int* avgY);

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

void extractRGB(IplImage* img)
{
	CvSize size = cvGetSize(img);
	int gap = size.height / 3;

	CvSize dstSize = cvSize(size.width, gap);
	IplImage* dst = cvCreateImage(size, 8, 3);

	int* avgX, *avgY;


	brightnessAvgX(img, "avgX",avgX);
	brightnessAvgY(img, "avgY",avgY);


	int sum = 0;
	CvScalar c, out;
	int numBlue, numGreen, numRed;
	for (int y = 0; y < gap; y++)
	{
		for (int x = 0; x < size.width; x++)
		{ 
			int yBlue = y;
			CvScalar scaleBlue = cvGet2D(img, yBlue, x);
			numBlue = scaleBlue.val[2];

			int yGreen = y+gap;
			CvScalar scaleGreen = cvGet2D(img, yGreen, x);
			numGreen = scaleGreen.val[1];

			int yRed = y+gap*2;
			CvScalar scaleRed = cvGet2D(img, yRed, x);
			numRed = scaleRed.val[0];

			out = cvScalar(numRed, numGreen, numBlue);

			cvSet2D(dst, y, x, out);
		}
	}
	cvShowImage("asdf", dst);
}

void brightnessAvgX(IplImage* img, const char* name, int* avgX)
{
	IplImage* dst = cvCreateImage(cvSize(img->width, 255), 8, 3);
	avgX = (int*)malloc(sizeof(int) * img->width);
	CvSize size = cvGetSize(img);

	int sum;
	int avg;
	for (int x = 0; x < size.width; x++)
	{
		avg = 0;
		for (int y = 0; y < size.height; y++)
		{
			sum = 0;
			CvScalar c = cvGet2D(img, y, x);
			for (int k = 0; k < 3; k++)
			{
				sum += c.val[k];
			}
			sum /= 3;
			avg += sum;
		}
		avg/= size.height;
		avgX[x] = avg;
		cvSet2D(dst, 255- avg, x, cvScalar(255, 255, 255));
		printf("x: %d , avgBrightness : %d\n", x, avg);

	}
	cvShowImage("dst", dst);
}

void brightnessAvgY(IplImage* img, const char* name, int* avgY)
{
	IplImage* dst = cvCreateImage(cvSize(img->height,255), 8, 3);

	avgY = (int*)malloc(sizeof(int) * img->height);

	CvSize size = cvGetSize(img);

	int sum;
	int avg;
	for (int y = 0; y < size.height; y++)
	{
		avg = 0;
		for (int x = 0; x < size.width; x++)
		{
			sum = 0;
			CvScalar c = cvGet2D(img, y, x);
			for (int k = 0; k < 3; k++)
			{
				sum += c.val[k];
			}
			sum /= 3;
			avg += sum;
		}
		avg /= size.width;
		avgY[y] = avg;
		cvSet2D(dst, 255 - avg,y , cvScalar(255, 255, 255));
		printf("y: %d , avgBrightness : %d\n", y, avg);

	}
	showImageFit(name, dst);
}

void swap(int* a, int* b)
{
	int t = *a;
	*a = *b;
	*b = t;
}

int partition(int arr[], int low, int high)
{
	int pivot = arr[high];
	int i = (low - 1);

	for (int j = low; j <= high - 1; j++)
	{
		if (arr[j] < pivot)
		{
			i++;
			swap(&arr[i], &arr[j]);
		}
	}
	swap(&arr[i + 1], &arr[high]);
	return (i + 1);
}

void quickSort(int arr[], int low, int high)
{
	if (low < high)
	{
		int pi = partition(arr, low, high);

		quickSort(arr, low, pi - 1);
		quickSort(arr, pi + 1, high);
	}
}

int main()
{
	IplImage* src1 = cvLoadImage("c:\\MultiMedia\\AS2\\pg1.jpg");
	IplImage* src2 = cvLoadImage("c:\\MultiMedia\\AS2\\pg2.jpg");
	IplImage* src3 = cvLoadImage("c:\\MultiMedia\\AS2\\pg3.jpg");
	IplImage* src4 = cvLoadImage("c:\\MultiMedia\\AS2\\pg4.jpg");

	extractRGB(src1);

	showImageFit("src1", src1);
	/*showImageFit("src3", src3);
	showImageFit("src2", src2);
	showImageFit("src4", src4);*/
	
	cvWaitKey();
}