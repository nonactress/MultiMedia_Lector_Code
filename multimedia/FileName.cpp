#include<opencv2/opencv.hpp>

int main()
{
	CvSize size = cvSize(512,256);
	/*
	size.width = 800;
	size.height = 600;*/
	IplImage* img = cvCreateImage(size, 8, 3);

	for (int y = 0; y < size.height; y++)
	{
		for (int x = 0; x < size.width; x++)
		{
			cvSet2D(img, y, x, cvScalar(255, 255, 255));
		}
	}

	//cvSet(img,cvScalar(0, 0, 255)); 위 이중 for문 과 동일 
	
	for(int y=0;y<size.height;y++)
		for (int x = 0; x < size.width; x++)
		{
			float nx = x / float(size.width);
			float ny = y / float(size.height);



			/*float cx = size.width / 2;
			float cy = size.height / 2;

			
			float dist = sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));

			float alpha = dist / float(size.width*sqrt(2)/2);*/

			CvScalar c1 = cvScalar(255, 0, 0);
			CvScalar c2 = cvScalar(0, 0, 255);

			float alpha = (nx + ny) / float(1);
			float smAlpha = alpha * alpha;

			//CvScalar c = alpha * c1 + (1 - alpha) * c2;
			CvScalar c;
			for (int k = 0; k < 3; k++)
				c.val[k] = (1 - xmAlpha) * c1.val[k] + smlpha * c2.val[k];

			cvSet2D(img, y, x, c);
		}

	cvShowImage("image", img);
	cvWaitKey();

}