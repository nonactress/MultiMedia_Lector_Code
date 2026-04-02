#include<opencv2/opencv.hpp>

// Partition function: divide array based on pivot
int partition(double arr[], int low, int high)
{
	double pivot = arr[high];
	int i = low - 1;

	for (int j = low; j < high; j++)
	{
		if (arr[j] < pivot)
		{
			i++;
			double temp = arr[i];
			arr[i] = arr[j];
			arr[j] = temp;
		}
	}
	double temp = arr[i + 1];
	arr[i + 1] = arr[high];
	arr[high] = temp;

	return i + 1;
}

// Quick Sort function
void quickSort(double arr[], int low, int high)
{
	if (low < high)
	{
		int pi = partition(arr, low, high);
		quickSort(arr, low, pi - 1);
		quickSort(arr, pi + 1, high);
	}
}
int getBrightness(CvScalar c)
{
	int total = 0;
	for (int i = 0; i < 3; i++)
	{
		total += c.val[i];
	}
	return total;
}

// Swap two CvScalar values
void cvSwap(CvScalar* a, CvScalar* b)
{
	CvScalar temp = *a;
	*a = *b;
	*b = temp;
}


int main()
{
	IplImage* src = cvLoadImage("c:\\temp\\lena.png");
	CvSize size = cvGetSize(src);
	IplImage* dst = cvCreateImage(size, 8, 3);
	IplImage* blur = cvCreateImage(size, 8, 3);
	IplImage* diff = cvCreateImage(size, 8, 3);

	cvSmooth(src, blur, CV_GAUSSIAN, 21);


	for (int y = 0; y < size.height; y++)
		for (int x = 0; x < size.width; x++)
		{
			CvScalar f = cvGet2D(src, y, x);
			CvScalar g;
			for (int i = 0; i < 3; i++)
			{
			  g.val[i] = getBrightness(f) / 3;
			}
			
		}

	for(int y=0;y<size.height;y++)
		for (int x = 0; x < size.width; x++)
		{
			CvScalar A = cvGet2D(src, y, x);
			CvScalar B = cvGet2D(blur, y, x);
			CvScalar D;
			for (int k = 0; k < 3; k++)
			{
				D.val[k] = A.val[k] - B.val[k] + 128;
			}
			
			cvSet2D(diff, y, x, D);

		}

	float alpha = 1.0f;
	while (true)
	{
		for (int y = 0; y < size.height; y++)
			for (int x = 0; x < size.width; x++)
			{
				CvScalar D = cvGet2D(diff, y, x);
				CvScalar B = cvGet2D(blur, y, x);
				CvScalar g;
				for (int k = 0; k < 3; k++)
				{
					g.val[k] = B.val[k] + alpha * (D.val[k] - 128);
				}
				cvSet2D(dst, y, x, g);
			}

		cvShowImage("src", src);
		cvShowImage("blur", blur);
		cvShowImage("diff", diff);
		cvShowImage("dst", dst);
		cvWaitKey();
		alpha += 0.1;
	}
	


	
	
	



	/*const int k = 1;
	const int NUM = (2 * k + 1)*(2*k+1);
	for(int y=0;y<size.height;y++)
		for (int x = 0; x < size.width; x++)
		{
			CvScalar f[NUM];
			int cnt = 0;
			

			for(int v=-k;v<=k;v++)
				for (int u = -k; u <= k; u++)
				{
					f[cnt++] = cvGet2D(src, y + v, x + u);
				}
			for (int n = 0; n < cnt-1; n++)
			{
				if(getBrightness(f[n])< getBrightness(f[n+1]))
				cvSwap(&f[n],&f[n+1]);
			}
			
			CvScalar g = f[NUM/2];
			cvSet2D(dst, y, x, g);
		}

	cvSmooth(src, dst, CV_MEDIAN, 3); // smoothing 필터
	cvSmooth(src, dst, CV_BLUR, 200); // 평균 필터
	

	cvShowImage("src", src);
	cvShowImage("dst", dst);
	cvWaitKey();*/


}