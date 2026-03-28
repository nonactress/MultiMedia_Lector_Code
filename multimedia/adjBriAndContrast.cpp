#include <opencv2/opencv.hpp>

CvScalar adjBriAndContrast(CvScalar in, float a, float b)
{
	CvScalar out;
	for (int k = 0; k < 3; k++)
		out.val[k] = a * in.val[k] + b;

	return out;
}

void myDrawGraph(IplImage * img, float a, float b)
{
	cvSet(img, cvScalar(255, 255, 255));
	float prev_y;
	for (int x = 0; x < 256; x++)
	{
		float y = 255 - (a * x + b);
		if (x == 0) prev_y = y;

		if (y<0 || y>img->height - 1) continue;
		if (x<0 || x>img->width - 1) continue;

		int y1 = prev_y;
		int y2 = y;
		if (y1 > y2) {
			int temp = y1;
			y1 = y2;
			y2 = temp;
		}

		for(int iy = y1; iy<y2; iy++)
			cvSet2D(img, iy, x, cvScalar(0, 0, 0));
		
		prev_y = y;

	}
	cvShowImage("graph", img);
}



int main()
{
	IplImage * src = cvLoadImage("c:\\temp\\lena.png");
	CvSize size = cvGetSize(src);
	IplImage * dst = cvCreateImage(size, 8, 3);
	IplImage * graph = cvCreateImage(cvSize(256, 256), 8, 3);

	float a = 1.0f;		// 기울기
	float b = 0.0f;		// y 절편

	while (true)
	{
		
		myDrawGraph(graph, a, b);

		printf("a=%f, b= %f \n", a, b);

		cvCopy(src, dst);

		for (int y = 0; y < size.height; y++)
			for (int x = size.width / 2; x < size.width; x++)
			{
				CvScalar f = cvGet2D(src, y, x);
				CvScalar g = adjBriAndContrast(f, a, b);
				cvSet2D(dst, y, x, g);
			}
		cvShowImage("dst", dst);
		int key = cvWaitKey();

		switch (key) {
		case '1':			b += 10;	break;
		case '2':			b -= 10;	break;
		case '3':			a += 0.1f;	break;
		case '4':			a -= 0.1f;	break;
		case 'q':			return 0;	break;
		default:						break;
		}
	}


	return 0;
}