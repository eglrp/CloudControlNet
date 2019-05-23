#include "imageprocessing.h"

bool Imageprocess::findCorrespondingPointsbyORB(const Mat& img1, const Mat& img2, vector<cv::Point2f>& points1, vector<cv::Point2f>& points2)
{
	cv::ORB orb;
	vector<cv::KeyPoint> kp1, kp2;
	cv::Mat desp1, desp2;
	orb(img1, cv::Mat(), kp1, desp1);
	orb(img2, cv::Mat(), kp2, desp2);
	cout << "Found" << kp1.size() << " and " << kp2.size() << " feature points" << endl;

	cv::Ptr<cv::DescriptorMatcher>  matcher = cv::DescriptorMatcher::create("BruteForce-Hamming");

	double knn_match_ratio = 0.8;
	vector< vector<cv::DMatch> > matches_knn;
	matcher->knnMatch(desp1, desp2, matches_knn, 2);
	vector< cv::DMatch > matches;
	for (size_t i = 0; i < matches_knn.size(); i++)
	{
		if (matches_knn[i][0].distance < knn_match_ratio * matches_knn[i][1].distance)
			matches.push_back(matches_knn[i][0]);
	}

	if (matches.size() <= 15) //Too few match number
		return false;

	for (auto m : matches)
	{
		points1.push_back(kp1[m.queryIdx].pt);
		points2.push_back(kp2[m.trainIdx].pt);
	}

	return true;
}


Mat Imageprocess::Sobelboundary(Mat img0)
{
	//Using Sobel Operation
	Mat grad_xg, grad_yg, abs_grad_xg, abs_grad_yg, dstg;

	//��ԭʼͼת��Ϊ�Ҷ�ͼ
	//cvtColor(img0, grayImage, COLOR_BGR2GRAY);
	//��x�����ݶ�
	Sobel(img0, grad_xg, CV_16S, 1, 0, 3, 1, 1, BORDER_DEFAULT);
	convertScaleAbs(grad_xg, abs_grad_xg);
	//��y�����ݶ�S
	Sobel(img0, grad_yg, CV_16S, 0, 1, 3, 1, 1, BORDER_DEFAULT);
	convertScaleAbs(grad_yg, abs_grad_yg);
	//�ϲ��ݶ�
	addWeighted(abs_grad_xg, 0.5, abs_grad_yg, 0.5, 0, dstg);
	return dstg;
}


float Imageprocess::caculateCurrentEntropy(Mat hist, int threshold)
{
	float BackgroundSum = 0, targetSum = 0;
	const float* pDataHist = (float*)hist.ptr<float>(0);
	for (int i = 0; i < 256; i++)
	{
		//�ۼƱ���ֵ
		if (i < threshold)
		{
			BackgroundSum += pDataHist[i];
		}
		//�ۼ�ǰ��ֵ
		else
		{
			targetSum += pDataHist[i];
		}
	}
	float BackgroundEntropy = 0, targetEntropy = 0;
	for (int i = 0; i < 256; i++)
	{
		//���㱳����
		if (i < threshold)
		{
			if (pDataHist[i] == 0)
				continue;
			float ratio1 = pDataHist[i] / BackgroundSum;
			BackgroundEntropy += -ratio1*logf(ratio1);
		}
		else  //����ǰ����
		{
			if (pDataHist[i] == 0)
				continue;
			float ratio2 = pDataHist[i] / targetSum;
			targetEntropy += -ratio2*logf(ratio2);
		}
	}
	return (targetEntropy + BackgroundEntropy);  //�Ӻͣ��õ���ǰ��ֵ����
}


Mat Imageprocess::maxEntropySegMentation(Mat inputImage)
{
	// Max Entropy Binarization 
	// Using the distribution of histogram to calculate the threshold leading to the max entropy.
	const int channels[1] = { 0 };
	const int histSize[1] = { 256 };
	float pranges[2] = { 0, 256 };
	const float* ranges[1] = { pranges };
	MatND hist;
	calcHist(&inputImage, 1, channels, Mat(), hist, 1, histSize, ranges);
	float maxentropy = 0;
	int max_index = 0;
	Mat result;
	for (int i = 0; i < 256; i++)  //����256��ֵ��Ϊ��ֵ�� ��ȡ�����ֵ
	{
		float cur_entropy = caculateCurrentEntropy(hist, i);
		if (cur_entropy > maxentropy)
		{
			maxentropy = cur_entropy;
			max_index = i;
		}
	}
	threshold(inputImage, result, max_index, 1, CV_THRESH_BINARY);  // > max_index assign as 1   < max_index assign as 0
	return result;
}

void Imageprocess::CcaByTwoPass(const Mat & _binImg, Mat & _labelImg)
{
	// connected component analysis (8-component)
	// use two-pass algorithm ����ɨ�跨
	// 1. first pass: label each foreground pixel with a label
	// 2. second pass: visit each labeled pixel and merge neighbor labels
	// 
	// foreground pixel: _binImg(x,y) = 1
	// background pixel: _binImg(x,y) = 0

	//  reference: https://blog.csdn.net/icvpr/article/details/10259577 


	if (_binImg.empty() ||
		_binImg.type() != CV_8UC1)
	{
		return;
	}

	// 1. first pass

	_labelImg.release();
	_binImg.convertTo(_labelImg, CV_32SC1);                             // _labelImg -> _binImg  32 Signed Ϊ���ܷ��㹻������label������������32λ��

	int label = 1;                                                      // start by 2
	vector<int> labelSet;                                               // ������label
	labelSet.push_back(0);                                              // background: 0
	labelSet.push_back(1);                                              // foreground: 1

	int rows = _binImg.rows - 1;
	int cols = _binImg.cols - 1;
	for (int i = 1; i < rows; i++)                                      // �б���
	{
		int* data_preRow = _labelImg.ptr<int>(i - 1);                   // ָ����һ��
		int* data_curRow = _labelImg.ptr<int>(i);                       // ָ����һ��
		for (int j = 1; j < cols; j++)                                  // �б���
		{
			if (data_curRow[j] == 1)                                    // �� ��ǰ�� ������ ��ǰ�� �����ǻ�û��label��
			{
				vector<int> neighborLabels;                             // ��������label����
				neighborLabels.reserve(2);                              // Ԥ���ռ�
				int leftPixel = data_curRow[j - 1];                     // ����������
				int upPixel = data_preRow[j];                           // ����������
				//int leftupPixel = data_preRow[j - 1];                   // ������������

				if (leftPixel > 1)                                      // �������������Լ���label
				{
					neighborLabels.push_back(leftPixel);                // ����label �������label�ӽ�ȥ
				}
				if (upPixel > 1)                                        // �������������Լ���label
				{
					neighborLabels.push_back(upPixel);                  // ����label �������label�ӽ�ȥ
				}
				/*if (leftupPixel > 1)                                    // ���������������Լ���label
				{
				neighborLabels.push_back(leftPixel);                // ����label ���������label�ӽ�ȥ
				}*/

				if (neighborLabels.empty())                             // �����򶼻�ûlabel 
				{
					labelSet.push_back(++label);                        // assign to a new label
					data_curRow[j] = label;                             // ��ǰ���ر�Ϊ��label
					labelSet[label] = label;
				}
				else                                                    // ��Ȼ �����������label�ˣ�
				{
					sort(neighborLabels.begin(), neighborLabels.end()); // ����label���� ��С������
					int smallestLabel = neighborLabels[0];
					data_curRow[j] = smallestLabel;                     // ��ǰ���ظ���С��label

					// save equivalence
					for (size_t k = 1; k < neighborLabels.size(); k++)  // ��������label��
					{
						int tempLabel = neighborLabels[k];              // 
						int& oldSmallestLabel = labelSet[tempLabel];
						if (oldSmallestLabel > smallestLabel)
						{
							labelSet[oldSmallestLabel] = smallestLabel;
							oldSmallestLabel = smallestLabel;
						}
						else if (oldSmallestLabel < smallestLabel)
						{
							labelSet[smallestLabel] = oldSmallestLabel;
						}
					}
				}
			}
		}
	}

	// update equivalent labels 
	// assigned with the smallest label in each equivalent label set  
	// ��¼Neighbors�и���ֵ��label��֮�����ȹ�ϵ������Щֵ��label��ͬ��ͬһ����ͨ����  
	for (size_t i = 2; i < labelSet.size(); i++)  // 0,1 ����
	{
		int curLabel = labelSet[i];
		int preLabel = labelSet[curLabel];
		while (preLabel != curLabel)
		{
			curLabel = preLabel;
			preLabel = labelSet[preLabel];
		}
		labelSet[i] = curLabel;
	}


	// 2. second pass
	for (int i = 0; i < rows; i++)
	{
		int* data = _labelImg.ptr<int>(i);
		for (int j = 0; j < cols; j++)
		{
			int& pixelLabel = data[j];
			pixelLabel = labelSet[pixelLabel];
		}
	}
	//totallabel = label;
	//cout << "Number label: " << totallabel << endl;
}


void Imageprocess::CcaBySeedFill(const Mat& _binImg, Mat& _lableImg)
{
	// connected component analysis (8-component)
	// use seed filling algorithm
	// 1. begin with a foreground pixel and push its foreground neighbors into a stack;
	// 2. pop the top pixel on the stack and label it with the same label until the stack is empty
	// 
	// foreground pixel: _binImg(x,y) = 1
	// background pixel: _binImg(x,y) = 0

	// ע��������䷨Ҫ���Եһ�����ض���Ϊ0��Ϊ1�Ļ����±�Խ����

	if (_binImg.empty() ||
		_binImg.type() != CV_8UC1)
	{
		cout << "Wrong type" << endl;
		return;
	}

	_lableImg.release();
	_binImg.convertTo(_lableImg, CV_32SC1);    //_labelImg �� CV_32SC1��

	int label = 1;  // start by 2
	//vector<vector<pair<int, int>>> labeledPixel;
	//labeledPixel.resize(10000); // ����д��̫�Ͻ�����Ϊ���10000����

	int rows = _binImg.rows;
	int cols = _binImg.cols;
	for (int i = 1; i < rows - 1; i++)
	{
		int* data = _lableImg.ptr<int>(i);
		for (int j = 1; j < cols - 1; j++)
		{
			if (data[j] == 1)
			{
				std::stack<std::pair<int, int>> neighborPixels;
				neighborPixels.push(std::pair<int, int>(i, j));     // pixel position: <i,j>
				++label;  // begin with a new label
				while (!neighborPixels.empty())
				{
					// get the top pixel on the stack and label it with the same label
					std::pair<int, int> curPixel = neighborPixels.top();
					int curX = curPixel.first;
					int curY = curPixel.second;
					_lableImg.at<int>(curX, curY) = label;

					//pair<int, int> pixelcor(curX, curY);
					//labeledPixel[label].push_back(pixelcor);


					// pop the top pixel
					neighborPixels.pop();

					// push the 8-neighbors (foreground pixels)
					if (_lableImg.at<int>(curX, curY - 1) == 1)
					{// left pixel
						neighborPixels.push(std::pair<int, int>(curX, curY - 1));
					}
					if (_lableImg.at<int>(curX, curY + 1) == 1)
					{// right pixel
						neighborPixels.push(std::pair<int, int>(curX, curY + 1));
					}
					if (_lableImg.at<int>(curX - 1, curY) == 1)
					{// up pixel
						neighborPixels.push(std::pair<int, int>(curX - 1, curY));
					}
					if (_lableImg.at<int>(curX + 1, curY) == 1)
					{// down pixel
						neighborPixels.push(std::pair<int, int>(curX + 1, curY));
					}
					if (_lableImg.at<int>(curX - 1, curY - 1) == 1)
					{// left up pixel
						neighborPixels.push(std::pair<int, int>(curX - 1, curY - 1));
					}
					if (_lableImg.at<int>(curX - 1, curY + 1) == 1)
					{// left down pixel
						neighborPixels.push(std::pair<int, int>(curX - 1, curY + 1));
					}
					if (_lableImg.at<int>(curX + 1, curY - 1) == 1)
					{// right up pixel
						neighborPixels.push(std::pair<int, int>(curX + 1, curY - 1));
					}
					if (_lableImg.at<int>(curX + 1, curY + 1) == 1)
					{// right down pixel
						neighborPixels.push(std::pair<int, int>(curX + 1, curY + 1));
					}
				}
			}
		}
	}
	/*int labelnumber=label-1;
	for (int m = 2; m <= label; m++){
	if (labeledPixel[m].size() < K){
	for (int n = 0; n < labeledPixel[m].size(); n++){
	int del_i = labeledPixel[m][n].first;
	int del_j = labeledPixel[m][n].second;
	_lableImg.at<int>(del_i, del_j) = 0;
	}
	labelnumber--;
	}
	}*/
	//totallabel = label;

	//cout << "Number label: " << totallabel << endl;
}
Scalar Imageprocess::GetRandomColor()
{
	uchar r = 255 * (rand() / (1.0 + RAND_MAX));   // rand() / (1.0+ RAND_MAX) : a random float number between 0 and 1 (can't be equal to 1)
	uchar g = 255 * (rand() / (1.0 + RAND_MAX));   // rand() 0 ~ 0x7fff ��32767��  RAND_MAX = 32767
	uchar b = 255 * (rand() / (1.0 + RAND_MAX));
	return Scalar(b, g, r);
}


void Imageprocess::LabelColor(const Mat & _labelImg, Mat & _colorLabelImg)
{
	if (_labelImg.empty() ||
		_labelImg.type() != CV_32SC1)
	{
		return;
	}

	std::map<int, Scalar> colors;  //ӳ��

	//labelx.resize(totallabel - 1);
	//labely.resize(totallabel - 1);

	int rows = _labelImg.rows;
	int cols = _labelImg.cols;

	_colorLabelImg.release();
	_colorLabelImg.create(rows, cols, CV_8UC3);
	_colorLabelImg = Scalar::all(0);

	for (int i = 0; i < rows; i++)
	{
		const int* data_src = (int*)_labelImg.ptr<int>(i);
		uchar* data_dst = _colorLabelImg.ptr<uchar>(i);
		for (int j = 0; j < cols; j++)
		{
			int pixelValue = data_src[j];
			if (pixelValue > 1)
			{
				//if(j%100==0) cout << pixelValue << endl;
				//labelx[pixelValue - 2].push_back(i); // ȷ����Label��ռ�ݵ�����֮x��y
				//labely[pixelValue - 2].push_back(j);

				if (colors.count(pixelValue) <= 0)
				{
					colors[pixelValue] = GetRandomColor();
				}
				Scalar color = colors[pixelValue];
				*data_dst++ = color[0];
				*data_dst++ = color[1];
				*data_dst++ = color[2];
			}
			else
			{
				data_dst++;
				data_dst++;
				data_dst++;
			}
		}
	}
}
void Imageprocess::DetectCornerHarris(const Mat & src, const Mat & colorlabel, Mat & cornershow, Mat & cornerwithimg, int threshold)
{
	Mat corner, corner8u, imageGray;
	src.convertTo(imageGray, CV_8UC1);
	corner = Mat::zeros(src.size(), CV_32FC1);
	cornerHarris(imageGray, corner, 3, 3, 0.04, BORDER_DEFAULT);
	normalize(corner, corner8u, 0, 255, CV_MINMAX);  //��һ��
	convertScaleAbs(corner8u, cornershow);
	cornerwithimg = colorlabel.clone();
	for (int i = 0; i < src.rows; i++)
	{
		for (int j = 0; j < src.cols; j++)
		{
			if (cornershow.at<uchar>(i, j)>threshold)  //��ֵ�ж�
			{
				circle(cornerwithimg, Point(j, i), 2, Scalar(255, 255, 255), 2); //��ע�ǵ�
			}
		}
	}
}
//bug�ĸ�Դ����CCA �������Ա�Ե���صĴ����±�Խ��
void Imageprocess::ImgReverse(const Mat &img, Mat &img_reverse){
	img.convertTo(img_reverse, CV_8UC1);

	for (int i = 1; i < img.rows - 1; i++)
	{
		for (int j = 1; j < img.cols - 1; j++)
		{
			if (img.at<uchar>(i, j) == 1)img_reverse.at<uchar>(i, j) = 0;
			if (img.at<uchar>(i, j) == 0)img_reverse.at<uchar>(i, j) = 1;
		}
	}
	//imshow("imgReverse", 255*img_reverse);
}
void Imageprocess::ImgFilling(const Mat &img, Mat &img_fill)
{
	img.convertTo(img_fill, CV_8UC1);

	Mat img_reverse, img_reverse_label;
	//threshold(img, img_reverse, 1, 1, CV_THRESH_BINARY);
	ImgReverse(img, img_reverse);
	CcaBySeedFill(img_reverse, img_reverse_label);

	for (int i = 1; i < img.rows - 1; i++)
	{
		for (int j = 1; j < img.cols - 1; j++)
		{
			if (img_reverse_label.at<int>(i, j) == 2)
				img_reverse.at<uchar>(i, j) = 0;
			img_fill.at<uchar>(i, j) = img.at<uchar>(i, j) + img_reverse.at<uchar>(i, j);

		}
	}

	//imshow("imgfill", 255 * img_fill);
}

void Imageprocess::DetectCornerShiTomasi(const Mat & src, const Mat & colorlabel, Mat & cornerwithimg, int minDistance, double qualityLevel)
{
	Mat imageGray;
	src.convertTo(imageGray, CV_8UC1);
	vector<Point2f> corners;
	int maxCorners = INT_MAX;
	int blockSize = 3;
	bool useHarrisDetector = false;
	double k = 0.04;

	cornerwithimg = colorlabel.clone();
	/// Apply corner detection :Determines strong corners on an image.
	goodFeaturesToTrack(imageGray, corners, maxCorners, qualityLevel, minDistance, Mat(), blockSize, useHarrisDetector, k);

	// Draw corners detected
	for (int i = 0; i < corners.size(); i++){
		circle(cornerwithimg, corners[i], 3, Scalar(255, 255, 255), 1, 8, 0);
	}
}