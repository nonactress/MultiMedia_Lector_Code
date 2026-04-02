#include <opencv2/opencv.hpp>
int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\lena.png");
	
	CvSize size = cvGetSize(src);
	
	IplImage* dst = cvCreateImage(size, 8, 3);

	int batch = 1;
	int windowSize = pow((2 * batch + 1), 2);


	float h[3][3] = {
		/*{1 / 9.0,1 / 9.0,1 / 9.0},
		{1 / 9.0,1 / 9.0,1 / 9.0},
		{1 / 9.0,1 / 9.0,1 / 9.0}*/
		{1/16.0,2/16.0,1/16.0},
		{2/16.0,4/16.0,2/16.0},
		{1/16.0,2/16.0,1/16.0}
	};
	for(int y= batch;y<size.height- batch;y++)
		for (int x = batch; x < size.width- batch; x++)
		{
			CvScalar g = cvScalar(0, 0, 0);
			for (int v= -batch; v <= batch; v++)
			{
				for (int u = -batch; u <= batch; u++)
				{
					CvScalar c = cvGet2D(src, y+v, x+u);
					for (int k = 0; k < 3; k++)
						g.val[k] += c.val[k]*h[k+v][k+u];
				}
			}
			cvSet2D(dst, y, x, g);
		}


	cvShowImage("src", src);
	cvShowImage("dst", dst);
	cvWaitKey();
	return 0;
}