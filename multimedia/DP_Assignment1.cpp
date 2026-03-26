#include<opencv2/opencv.hpp>
void Prob_1(IplImage* img)
{
	CvSize size = cvGetSize(img);
	IplImage* dst = cvCreateImage(size, 8, 3);
	CvScalar c;
	for(int y=0;y<size.height;y++)
		for (int x = 0; x < size.width; x++)
		{
			c = cvGet2D(img, y, x);
			cvSet2D(dst, size.height - y-1, size.width - x-1, c);
		}
	cvShowImage("Prob_1", dst);
}
CvScalar gray_scale(CvScalar c);

float getNormalizedDist(float x, float y) {
	return sqrt((x) * (x) + (y) * (y));
}

void Prob_5(IplImage* img)
{
	CvSize size = cvGetSize(img);
	IplImage* dst = cvCreateImage(size, 8, 3);
	CvScalar c;
	double nx;
	double ny;
	double dist;

	for (int y = 0; y < size.height; y++)
		for (int x = 0; x < size.width; x++)
		{
			nx = (x / float(size.width - 1)) * 20 - 10;
			ny = (y / float(size.height - 1)) * 20 - 10;
			c = cvGet2D(img, y, x);
			dist = getNormalizedDist(nx, ny);
			c = ((int)dist % 2 != 0) ? cvScalar(0, 0, 0) : c;
			cvSet2D(dst, y, x, c);
		}
	cvShowImage("Prob_5", dst);
}

void Prob_6(IplImage* img)
{
	CvSize size = cvGetSize(img);
	IplImage* dst = cvCreateImage(size, 8, 3);
	CvScalar c;
	
	int nx;
	int ny;
	
	for (int y = 0; y < size.height; y++) {
		for (int x = 0; x < size.width; x++)
		{
			nx = (x / float(size.width)) * 10;
			ny = (y /float(size.height)) * 10;
			c = ((nx + ny) % 2 == 0)?cvScalar(0, 0, 0):cvGet2D(img, y, x);
			cvSet2D(dst, y, x, c);
		}
		
	}
	cvShowImage("Prob_6", dst);
}

void Prob_2(IplImage* img)
{
	CvSize size = cvGetSize(img);
	IplImage* dst = cvCreateImage(size, 8, 3);
	CvScalar c;
	int dx;

	for(int y=0;y<size.height;y++)
		for (int x = 0; x < size.width; x++)
		{
			dx = (x < (size.width / 2)) ? (x + (size.width / 2) ) : (x - (size.width / 2));
			c = cvGet2D(img, y, x);
			cvSet2D(dst, y, dx, c);
		}
	cvShowImage("Prob_2", dst);
}

CvScalar gray_scale(CvScalar c)
{
	int aver = (c.val[0] + c.val[1] + c.val[2])/3;
	return cvScalar(aver, aver, aver);
}

void Prob_4(IplImage* img)
{
	CvSize size = cvGetSize(img);
	IplImage* dst = cvCreateImage(size, 8, 3);
	CvScalar c;
	float nx;
	float ny;

	for (int y = 0; y < size.height; y++)
		for (int x = 0; x < size.width; x++)
		{
			nx = (x / float(size.width - 1)) * 2 - 1;
			ny = (y / float(size.height - 1)) * 2 - 1;

			c = cvGet2D(img, y, x);
			if(getNormalizedDist(nx,ny)<1.0)cvSet2D(dst, y, x, c);
			else cvSet2D(dst, y, x, gray_scale(c));
		}
	cvShowImage("Prob_4", dst);
}


void Prob_3(IplImage* img)
{
	CvSize size = cvGetSize(img);
	IplImage* dst = cvCreateImage(size, 8, 3);
	CvScalar c;
	float nx;
	float ny;
	
	for (int y = 0; y < size.height; y++)
		for (int x = 0; x < size.width; x++)
		{
			nx = (x / float(size.width-1))*2  - 1;
			ny = (y / float(size.height-1))*2 - 1;
	
			c = cvGet2D(img, y, x);
			//y + |x| =  +1; 
			nx = (nx > 0) ? nx : -nx;
			//y - |x|  = -1; 
			if ((nx + ny)<1 && (-nx + ny)>-1) cvSet2D(dst, y, x,c);
			else cvSet2D(dst, y, x, gray_scale(c));
		}
	cvShowImage("Prob_3", dst);
}

int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\sejong_small.jpg");
	Prob_1(src);
	Prob_2(src);
	Prob_3(src);
	Prob_4(src);
	Prob_5(src);
	Prob_6(src);
	cvWaitKey();
}