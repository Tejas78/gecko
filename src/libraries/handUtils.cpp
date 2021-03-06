//------------------------------------------------------------------------------
//-- handUtils
//------------------------------------------------------------------------------
//--
//-- A collection of useful functions for hand gesture recognition
//--
//------------------------------------------------------------------------------
//--
//-- This file belongs to the "Gecko - Gesture Recognition" project
//-- (https://github.com/David-Estevez/gecko)
//--
//------------------------------------------------------------------------------
//-- Authors: David Estevez Fernandez
//--          Irene Sanz Nieto
//--
//-- Released under the GPL license (more info on LICENSE.txt file)
//------------------------------------------------------------------------------

/*! \file handUtils.cpp
 *  \brief A collection of useful functions for hand gesture recognition
 *
 * \author David Estevez Fernandez ( http://github.com/David-Estevez )
 * \author Irene Sanz Nieto ( https://github.com/irenesanznieto )
 * \date Dec 12th, 2013
 */

#include "handUtils.h"

void drawCalibrationMarks( cv::Mat& input, cv::Mat& output, int halfSide, cv::Scalar color)
{
    //-- Get image dimensions
    int image_cols = input.cols;
    int image_rows = input.rows;

    //-- Draw a rectangle in the output image
    output = input.clone();
    cv::rectangle( output,
		   cv::Point( image_cols / 2 - halfSide,  image_rows/2 - halfSide ),
		   cv::Point( image_cols / 2 + halfSide,  image_rows/2 + halfSide),
		   color
		   );
}

void drawHistogram(const cv::Mat& img)
{
    int bins = 256;
    int nc = img.channels();
    std::vector<cv::Mat> hist( nc);
    std::vector<cv::Mat> canvas( nc);
    int hmax[3] = {0,0,0};

    for (int i = 0; i < hist.size(); i++)
	hist[i] = cv::Mat::zeros(1, bins, CV_32SC1);

    for (int i = 0; i < img.rows; i++)
    {
	for (int j = 0; j < img.cols; j++)
	{
	    for (int k = 0; k < nc; k++)
	    {
		uchar val = nc == 1 ? img.at<uchar>(i,j) : img.at<cv::Vec3b>(i,j)[k];
		hist[k].at<int>(val) += 1;
	    }
	}
    }

    for (int i = 0; i < nc; i++)
    {
	for (int j = 0; j < bins-1; j++)
	    hmax[i] = hist[i].at<int>(j) > hmax[i] ? hist[i].at<int>(j) : hmax[i];
    }

    const char* wname[3] = { "channel 1", "channel 2", "channel 3" };
    cv::Scalar colors[3] = { cv::Scalar(255,0,0), cv::Scalar(0,255,0), cv::Scalar(0,0,255) };

    for (int i = 0; i < nc; i++)
    {
	canvas[i] = cv::Mat::ones(125, bins, CV_8UC3);

	for (int j = 0, rows = canvas[i].rows; j < bins-1; j++)
	{
	    cv::line(
			canvas[i],
			cv::Point(j, rows),
			cv::Point(j, rows - (hist[i].at<int>(j) * rows/hmax[i])),
			nc == 1 ? cv::Scalar(200,200,200) : colors[i],
			1, 8, 0
			);
	}

	cv::imshow(nc == 1 ? "value" : wname[i], canvas[i]);
    }
}

void drawHistogramHSV(const cv::Mat &image)
{
    cv::Mat image_hsv;
    cv::cvtColor( image, image_hsv, CV_BGR2HSV);

    drawHistogram( image_hsv);
}


//-- Rectangle characterization:
double getAngle( cv::RotatedRect boundingRect)
{
    //-- Get points:
    cv::Point2f rect_points[4];
    boundingRect.points( rect_points );

    //-- Calculate side length:
    double length[4];

    for (int i = 0; i < 4; i++)
	if (i != 3)
	    length[i] = sqrt( pow(rect_points[i].x - rect_points[i+1].x, 2) +
			      pow(rect_points[i].y - rect_points[i+1].y, 2));
    //-- Find longest side:
   int longestId = 0;
   double longestValue = length[0];

   for (int i = 0; i < 4; i++)
       if (length[i] > longestValue)
	{
	    longestId = i;
	    longestValue = length[i];
	}

   //-- Calculate
   int vector_x = rect_points[longestId+1>3?0:longestId+1].x-rect_points[longestId].x;
   int vector_y = rect_points[longestId+1>3?0:longestId+1].y-rect_points[longestId].y;

   //-- Return angle
   return atan2( -vector_y , vector_x)*180/3.1415;
}

void filterContours( std::vector< std::vector<cv::Point> >& srcContours, std::vector< std::vector<cv::Point> >& handContour, const int min, const int max)
{


    std::vector<std::vector<cv::Point> > filteredContours;
    for (int i = 0; i <srcContours.size(); i++)
    if ( (int) srcContours[i].size() > min )//&& (int) srcContours[i].size() < max )
	    filteredContours.push_back( srcContours[i] );

    std::cout << "[Debug] Contours before: " << srcContours.size() << " Contours after: " << filteredContours.size() << std::endl;


    //-- Find largest contour:
    int largestId = 0;
    if ( (int) filteredContours.size() > 0)
    {

    int largestValue = (int) filteredContours[0].size();
    for (int i = 0; i < filteredContours.size(); i++)
	if ( (int) filteredContours[i].size() > largestValue )
	{
	    largestId = i;
	    largestValue = (int) filteredContours[i].size();
	}
    std::cout << "[Debug] Number of contours: " << (int) srcContours.size() << " Largest contour: " << (int) largestValue << std::endl;
    }
    else
	std::cerr << "All filtered!" << std::endl;


    handContour.clear();
    if ( (int) filteredContours.size() > 0)
	handContour.push_back( filteredContours[largestId]);
}


void backgroundSubs(cv::Mat & frame, backgroundSubstractor & bg)
{
    cv::Mat back;
    cv::Mat fore;
    bg(frame,fore);
    bg.getBackgroundImage(back);



//    Rectangular box: MORPH_RECT
//    Cross: MORPH_CROSS
//    Ellipse: MORPH_ELLIPSE

    int erosion_size=0.5;
    cv::Mat eroding_element = getStructuringElement(cv::MORPH_ELLIPSE,
                                            cv::Size( 2*erosion_size + 1, 2*erosion_size+1 ),
                                            cv::Point( erosion_size, erosion_size ) );

    int dilation_size=200;
    cv::Mat dilating_element = getStructuringElement(cv::MORPH_ELLIPSE,
                                            cv::Size( 2*erosion_size + 1, 2*erosion_size+1 ),
                                            cv::Point( erosion_size, erosion_size ) );

    cv::erode(fore,fore,eroding_element);
    cv::dilate(fore,fore,dilating_element);

//    cv::erode(fore,fore,cv::Mat());
//    cv::dilate(fore,fore,cv::Mat());

    frame.copyTo(fore,fore);

//    imshow("FORE", fore);

    fore.copyTo(frame);
}


float findAngle(cv::Point start, cv::Point end, cv::Point vertex)
{
    //-- Note:
    //-- The angle obtained here is the angle going from start-vertex segment to end-vertex segment

    //-- Find both vectors:
    cv::Vec2i u, v;
    u[0] = start.x - vertex.x;
    u[1] = start.y - vertex.y;

    v[0] = end.x - vertex.x;
    v[1] = end.y - vertex.y;

    //-- Find the angle [ acos( u · v / ( |u| · |v| )) ]
    float angle_rad = acos( (u[0]*v[0]+u[1]*v[1]) / ( norm(u) * norm(v) ) );

    return angle_rad*180/M_PI;
}


void printProgressBar(cv::Mat &src, cv::Mat &dst, float percentage, cv::Scalar color, int thickness)
{
    if ( dst.empty() )
        dst = src.clone();

    //-- Print progress on screen
    cv::Point start( thickness, dst.rows - thickness );
    cv::Point end( (dst.cols - 100)  * percentage + 50, dst.rows );
    cv::rectangle( dst, start, end, color, CV_FILLED );
}

