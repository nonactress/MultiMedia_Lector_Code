#include<opencv2/opencv.hpp>

float getDist(CvScalar a, CvScalar b)
{
	float sum = 0;
	for (int i = 0; i < 3; i++)
	{
		sum = (a.val[i] - b.val[i]) * (a.val[i] - b.val[i]);
	}

	return sum;
}

/*
int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\lena.png");


	CvSize size = cvGetSize(src);
	IplImage* dst = cvCreateImage(size, 8, 3);
	
	/*CvScalar pal[] = { cvScalar(0,0,0),
		cvScalar(64,64,64),
		cvScalar(128,128,128),
		cvScalar(92,92,92),
		cvScalar(255,255,255),cvScalar(192,192,192),
	cvScalar(255,0,0),cvScalar(0,255,0),cvScalar(0,0,255)};
	int num = 9;*/
/*
	CvScalar pal[] = {
	cvScalar(0, 0, 0),       // Black
	cvScalar(83, 37, 126),   // Dark Purple
	cvScalar(47, 82, 171),   // Brown
	cvScalar(161, 157, 194), // Light Gray
	cvScalar(241, 241, 255), // White
	cvScalar(79, 0, 255),    // Red
	cvScalar(0, 163, 255),   // Orange
	cvScalar(204, 204, 255)  // Peach (ÇÇºÎÅæ ´ë¿ë)
	};
	int num = 8;
	
	for(int y=0;y<size.height;y++)
		for (int x = 0; x < size.width; x++)
		{
			CvScalar f = cvGet2D(src, y, x);
			int bri = (f.val[0] + f.val[1] + f.val[2]) / 3;

			CvScalar g = pal[0];
			float minDist = FLT_MAX;
			for (int i = 0; i < num; i++)
			{
				float dist = getDist(pal[i], f);
				if (dist < minDist)
				{
					g = pal[i];
					minDist = dist;
				}
			}
			
			cvSet2D(dst, y, x,g);
		}

	cvShowImage("Src", src);
	cvShowImage("dst", dst);
	cvWaitKey();
	return 0;

}
*/
int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\lena.png");


	CvSize size = cvGetSize(src);
	IplImage* dst = cvCreateImage(size, 8, 3);
	IplImage* blur = cvCreateImage(size, 8, 3);
	cvSet(dst, cvScalar(0, 0, 0));
	int order[] = { 1,8,0,6,2,5,7,3,4};
	int num = 10;

	cvSmooth(src, blur, CV_BLUR, 5);

	for(int y=0;y<size.height-2;y+=3)
		for (int x = 0; x < size.width-2; x += 3) // 2Ä­¾¿ ¶Ù¹Ç·Î -1 ¹üÀ§ ÇØÁà¾ßÇÔ Áß¿ä 
		{
			int div = 255 / num;
			CvScalar f = cvGet2D(blur, y, x);
			int bri = (f.val[0] + f.val[1] + f.val[2]) / 3;

			int n = bri / div;
			if (n > 9)n = 9;
			for (int i = 0; i < n; i++)
			{
				int u = order[i]%3;
				int v = order[i]/3;

				cvSet2D(dst, y+u, x+v, cvScalar(255, 255, 255));
			}
		}

	cvShowImage("Src", src);
	cvShowImage("dst", dst);
	cvWaitKey();
	return 0;

}