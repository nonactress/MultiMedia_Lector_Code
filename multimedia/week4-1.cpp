#include<opencv2/opencv.hpp>
CvScalar adjGamma(CvScalar in, float gamma)
{
	CvScalar out;
	for (int k = 0; k < 3; k++)
	{
		out.val[k] = pow(in.val[k]/255.0, gamma)*255;
	}
	return out;
}
int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\lena.png");
	if (src == NULL)printf("null");
	CvSize size = cvGetSize(src);
	IplImage* dst = cvCreateImage(size, 8, 3);

	//cvCopy(src, dst);
	float gamma = 1.0f;
	while (1)
	{
		for (int y = 0; y < size.height; y++)
		{
			for (int x = 0; x < size.width; x++)
			{
				CvScalar f = cvGet2D(src, y, x);
				CvScalar g = adjGamma(f, gamma);
				cvSet2D(dst, y, x, g);
			}
		}
		cvShowImage("dst", dst);
		int key = cvWaitKey();
		switch (key) {
		case '1':
			gamma += 0.1f;
			break;
		case '2':
			gamma -= 0.1f; 
			break;
		case '3':
			break;
			break;
		}
	}
	
	





	cvShowImage("src", src);


}