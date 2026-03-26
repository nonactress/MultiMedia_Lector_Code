#include<opencv2/opencv.hpp>

void drawHisto(int histo[][3], int hMax , const char * name )
{
	int height = 400;
	IplImage* g = cvCreateImage(cvSize(256, height), 8, 3);
	cvSet(g, cvScalar(0, 0, 0));
	
	for (int k = 0; k < 3; k++)
	for (int bri = 0; bri < 256; bri++)
	{
		int cnt = histo[bri][k];
		float h = (cnt / (float)hMax) * height;
		if (h > height - 1)h = height - 1;

		
		for (int y = 0; y < h; y++)
		{
			CvScalar c = cvGet2D(g, height - y - 1, bri);
			c.val[k] = 255;

			cvSet2D(g, height-y-1, bri, c);
		}
	}
	cvShowImage(name, g);
}

void calculateHisto(IplImage* img, int histo[][3])
{


	for (int bri = 0; bri < 256; bri++)
	{
		histo[bri][0] = 0;
		histo[bri][1] = 0;
		histo[bri][2] = 0;
	}
			for (int y = 0; y < img->height; y++)
				for (int x = 0; x < img->width; x++)
				{
					CvScalar f = cvGet2D(img, y, x);
					for (int k = 0; k < 3; k++)
					{
						histo[(int)f.val[k]][k]++; // 초기화 설정 주의, 포인터로 해야하는 거 아닌가 
					}
				}
	


}

void calculateCumulative(int histo[][3], int cumul[][3])
{
	for (int k = 0; k < 3; k++)
	{
		cumul[0][k] = histo[0][k];
		for (int bri = 1; bri < 256; bri++)
		{
			cumul[bri][k] = cumul[bri - 1][k] + histo[bri][k];
		}
	}
}

void equalization(IplImage* src, IplImage* dst, int cumul[][3])
{
	CvSize size = cvGetSize(src);
	int cMax = size.width * size.height;
	for (int y = 0; y < size.height; y++)
	{
		for (int x = 0; x < size.width; x++)
		{
			CvScalar f = cvGet2D(src, y, x);
			CvScalar g;
			for (int k = 0; k < 3; k++)
			{
				g.val[k] = cumul[(int)f.val[k]][k]/(float) cMax * 255;
			}

			cvSet2D(dst, y, x, g);
		}
	}
}

int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\lena.png");
	CvSize size = cvGetSize(src);
	IplImage* dst = cvCreateImage(size, 8, 3);

	//1. calculate histo
	int histo[256][3];
	calculateHisto(src, histo);
	drawHisto(histo, size.width * size.height / 256 * 10, "histo");

	//2. cummulate histo
	int cumul[256][3];
	calculateCumulative(histo, cumul);
	drawHisto(cumul, size.width * size.height, "cumul");


	// 3. change colors based cummul histo
	equalization(src, dst, cumul);

	cvShowImage("src", src);
	cvShowImage("dst", dst);

	cvWaitKey();

	cvSaveImage("c:\\temp\\result.png",dst);
}