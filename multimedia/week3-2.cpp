#include<opencv2/opencv.hpp>

CvScalar adjBrightness(CvScalar src, float a, float b)
{
	CvScalar out;
	for (int k = 0; k < 3; k++)
	{
		out.val[k] = src.val[k] * a + b;
	}
	return out;
}

void printG(IplImage* img, float a, float b)
{
	cvSet(img, cvScalar(255, 255, 255));

	float prev_y;
	for (int x = 0; x < 256; x++)
	{
		float y = 255 - (a * x + b);
		if (x == 0)prev_y = y;
		if (y<0 || y>img->height - 1)continue;
		if (x<0 || x>img->width - 1)continue;

		for (float dy = prev_y; dy <= y; dy += 1.0) {
			cvSet2D(img, (int)dy, x, cvScalar(0, 0, 0));
			printf("%d", (int)dy);
		}

		prev_y = y;
	}
	cvShowImage("graph",img);
}

int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\lena.png");
	CvSize size = cvGetSize(src);
	IplImage* dst = cvCreateImage(size, 8, 3);
	IplImage* g = cvCreateImage(cvSize(256,256), 8, 3);

	float a = 1.0f;
	float b = 0.0f;

	

	while (true)
	{
		
		//cvCopy(src, dst);

		for (int y = 0; y < size.height; y++) {
			for (int x = 0; x < size.width; x++)
			{
				CvScalar f = cvGet2D(src, y, x);
				CvScalar g = adjBrightness(f, a, b);
				cvSet2D(dst, y, x, g);
			}
		}

		cvShowImage("src", src);
		cvShowImage("dst", dst);
		printG(dst, a, b);
		int key = cvWaitKey();
		switch (key) {
		case '1': b += 10; break;

		case '2': b -= 10; break;

		case '3': a *= 3; break;

		case '4': a /= 3; break;

		case '5': break; break;
		}
	}

	

}