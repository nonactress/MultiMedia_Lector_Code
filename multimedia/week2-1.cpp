#include<opencv2/opencv.hpp>

int main()
{
	IplImage* img = cvLoadImage("C:\\Users\\shjsh\\Downloads\\flower.jpg");
	IplImage* img2 = cvLoadImage("C:\\temp\\penguin.jpg");

	IplImage* tmp;

	if (img == nullptr)
	{
		printf("file not found");
		return -1;
	}
	

	// jpg(jpeg), bmp, 
	tmp = img;
	int ch;
	while (true)
	{
		cvShowImage("qwer", tmp);
		ch = cvWaitKey(-1);
		if (ch == 'q')break;
		else if (ch == '1')tmp = img;
		else if (ch == '2')tmp = img2;
		else tmp = (tmp != img) ? img : img2;


		switch (ch)
		{
		case 'q':


		case '1':


		case '2':

		}
	}
	/*cvShowImage("asdf", img);
	cvWaitKey();

	cvShowImage("agdf", img2);
	cvWaitKey();*/


	
	cvReleaseImage(&img);


	return 0;

}