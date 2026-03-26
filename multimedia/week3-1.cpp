#include<opencv2/opencv.hpp>

int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\lena.png");

	CvSize size = cvGetSize(src);

	IplImage* dst = cvCreateImage(size,8,3);

	for (int y = 0; y < size.height; y++)
		for (int x = 0; x < size.width; x++)
		{
			CvScalar c = cvGet2D(src, y, x);
			int bri = c.val[0] + c.val[1] + c.val[2];
			float alpha = bri / (255 * 3.0f);
			
			CvScalar f = cvScalar(58, 211, 255);
			CvScalar g;
			for (int k = 0; k < 3; k++)
			{
				g.val[k] = alpha * f.val[k];
			}
			cvSet2D(dst, y, x, g);
	
			
		}
	cvShowImage("asdf", src);

	cvShowImage("dst", dst);
	cvWaitKey();

}