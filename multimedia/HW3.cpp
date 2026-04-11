#include<opencv2/opencv.hpp>
#define max(x,y) ((x<y)?y:x)
void setTable(int* r[], int* g[], int* b[], IplImage* img);
CvScalar getSmooth(int x, int y, int* r[], int* g[], int* b[], IplImage* img, int k);
void myFastestMeanFilter(IplImage* src, IplImage* dst, int k)
{
	
	CvSize size = cvGetSize(src);
	int** sumTableR = (int**)malloc(sizeof(int*) * src->height);
	int** sumTableG = (int**)malloc(sizeof(int*) * src->height);
	int** sumTableB = (int**)malloc(sizeof(int*) * src->height);
	for (int i = 0; i < src->height; i++)
	{
		sumTableR[i] = (int*)malloc(sizeof(int) * src->width);
		sumTableG[i] = (int*)malloc(sizeof(int) * src->width);
		sumTableB[i] = (int*)malloc(sizeof(int) * src->width);
	}

	setTable(sumTableR, sumTableG, sumTableB, src);

	for (int y = 0; y < size.height; y++)
	{
		for (int x = 0; x < size.width; x++)
		{

			CvScalar c = getSmooth(x, y, sumTableR, sumTableG, sumTableB, src, k);
			cvSet2D(dst, y, x, c);
		}
	}
}
CvScalar getSmooth(int x,int y,int* r[], int* g[], int* b[], IplImage* img,int k)
{
	int offsetX = 0;
	int offsetY = 0;
	CvSize size = cvGetSize(img);

	int minDx = x - k/2;
	int maxDx = x + k/2;

	int minDy = y - k/2;
	int maxDy = y + k / 2;

	if (minDx < 0)
	{
		offsetX= -minDx;
		minDx = 0;
		
	}
	if (minDy < 0) { 
		offsetY = -minDy;
		minDy = 0; }

	if (maxDx > size.width - 1)
	{ 
		offsetX = maxDx - (size.width - 1);
		maxDx = size.width - 1;
	}
	if (maxDy > size.height - 1)
	{
		offsetY = maxDy - (size.height - 1);
		maxDy = size.height - 1;
	}

	int area = (k - offsetX) * (k - offsetY);
	if (area == 0) return cvScalar(0, 0, 0);

	int meanB = (b[maxDy][maxDx] - b[minDy][maxDx] - b[maxDy][minDx] + b[minDy][minDx]) / area;
	int meanG = (g[maxDy][maxDx] - g[minDy][maxDx] - g[maxDy][minDx] + g[minDy][minDx]) / area;
	int meanR = (r[maxDy][maxDx] - r[minDy][maxDx] - r[maxDy][minDx] + r[minDy][minDx]) / area;

	//if(x==0 && y==0 )printf("now pos(%d, %d) ,area %d  , offset : %d, k = %d \n", x, y, area, offset, k / 2);
	//if (offset != 0) printf("now pos(%d, %d) ,area %d  , offset : %d, k = %d \n", x,y,area , offset, k/2);
 
	return cvScalar(meanB, meanG, meanR);
}
void setTable(int* r[],int* g[],int *b[],IplImage* img)
{
	CvScalar c = cvGet2D(img, 0, 0);

	b[0][0] = c.val[0];
	g[0][0] = c.val[1];
	r[0][0] = c.val[2];

	for (int i = 1; i < img->width; i++)
	{
		CvScalar c = cvGet2D(img, 0, i);
		b[0][i] = c.val[0] + b[0][i-1];
		g[0][i] = c.val[1] + g[0][i - 1];
		r[0][i] = c.val[2] + r[0][i - 1];
	}

	for (int i = 1; i < img->height; i++)
	{
		CvScalar c = cvGet2D(img, i, 0);
		b[i][0] = c.val[0] + b[i - 1][0];
		g[i][0] = c.val[1] + g[i - 1][0];
		r[i][0] = c.val[2] + r[i - 1][0];
	}
	
	for (int v = 1; v < img->height; v++)
	{
		for (int u = 1; u < img->width; u++)
		{

			CvScalar c = cvGet2D(img, v, u);
			b[v][u] = c.val[0] + b[v - 1][u]+b[v][u-1]- b[v-1][u - 1];
			g[v][u] = c.val[1] + g[v - 1][u]+g[v][u-1] - g[v - 1][u - 1];;
			r[v][u] = c.val[2] + r[v - 1][u]+r[v][u-1] - r[v - 1][u - 1];;
		}
	}
}
int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\lena.png");
	CvSize size = cvGetSize(src);
	IplImage* cvdst = cvCreateImage(size, 8, 3);
	IplImage* dst = cvCreateImage(size, 8, 3);

	
	cvSmooth(src, cvdst, CV_BLUR, 50);
	myFastestMeanFilter(src, dst, 50);

	cvShowImage("src", src);
	cvShowImage("dst", dst);
	cvShowImage("cvdst", cvdst);
	cvWaitKey();
}