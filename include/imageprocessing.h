#ifndef IMAGE_H
#define IMAGE_H

#include <opencv2/opencv.hpp>   
#include <opencv2/highgui/highgui.hpp>
#include <array>
#include <stack>
#include <algorithm>
#include <numeric>

using namespace std;
using namespace cv;

class Imageprocess
{
public:
	bool findCorrespondingPointsbyORB(const cv::Mat& img1, const cv::Mat& img2, vector<cv::Point2f> & points1, vector<cv::Point2f>& points2);  //ORB_�Ҷ�Ӧ��;

	Mat Sobelboundary(Mat img0);                                                  //Sobel���ӱ�Ե��ȡ;
	Mat maxEntropySegMentation(Mat inputImage);                                   //�������ֵ�ָ�;
	void CcaByTwoPass(const Mat & _binfilterImg, Mat & _labelImg);                //����ɨ�跨��ͨ�ɷַ�����4����
	void CcaBySeedFill(const Mat& _binfilterImg, Mat & _lableImg);                //������䷨��ͨ�ɷַ�����8����
	void ImgReverse(const Mat &img, Mat &img_reverse);                            //��ֵ��ͼ��ɫ
	void ImgFilling(const Mat &img, Mat &img_fill);                               //�ն����
	void LabelColor(const Mat & _labelImg, Mat & _colorImg);                      //��ͨ����ɫ
	void DetectCornerHarris(const Mat & src, const Mat & colorlabel, Mat & cornershow, Mat & cornerwithimg, int threshold);             //Harris�ǵ���
	void DetectCornerShiTomasi(const Mat & src, const Mat & colorlabel, Mat & cornerwithimg, int minDistance, double mincorenerscore);  //Shi-Tomasi�ǵ���

protected:
	float caculateCurrentEntropy(Mat hist, int threshold);                        //���㵱ǰ��ֵ��ǰ����;
	Scalar GetRandomColor();                                                      //���ȡɫ��

private:
};

#endif //IMAGE_H