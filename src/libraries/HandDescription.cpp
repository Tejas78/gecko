#include "HandDescription.h"

//-- Initialization of the private parameters in the constructor
HandDescription:: HandDescription()
{
    //-- Initalize hand parameters
    //-----------------------------------------------------------------------
    _hand_angle=0;
    _hand_center = std::pair <int, int> (0, 0);
    _hand_bounding_box = cv::Rect();
    _hand_gesture = HAND_GESTURE_NONE;
    _hand_num_fingers = -1;
    _hand_found = false;


    //-- Kalman filter setup for estimating hand angle:
    //-----------------------------------------------------------------------

    //-- Create filter:
    kalmanFilterAngle.init( 2, 1, 0);
    kalmanFilterAngle.transitionMatrix = *( cv::Mat_<float>(2, 2) << 1, 1,
								     0, 1);
    //-- Initial state:
    kalmanFilterAngle.statePre.at<float>(0) = 90; //-- initial angle
    kalmanFilterAngle.statePre.at<float>(1) = 0;  //-- initial angular velocity
    
    //-- Set the rest of the matrices:
    cv::setIdentity( kalmanFilterAngle.measurementMatrix );
    cv::setIdentity( kalmanFilterAngle.processNoiseCov, cv::Scalar::all(0.0001));
    cv::setIdentity( kalmanFilterAngle.measurementNoiseCov, cv::Scalar::all(0.1));
    cv::setIdentity( kalmanFilterAngle.errorCovPost, cv::Scalar::all(0.1));



    //-- Kalman filter for estimating hand center
    //---------------------------------------------------------------------

    //-- Create filter:
    kalmanFilterCenter.init( 4, 2, 0);
    kalmanFilterCenter.transitionMatrix = *( cv::Mat_<float>(4, 4) << 1, 0, 1, 0,
								0, 1, 0, 1,
								0, 0, 1, 0,
								0, 0, 0, 1);
    //-- Get mouse position:
    //! \todo Change this for screen center?
    std::pair <int, int> initial_mouse = getMousePos( );

    //-- Initial state:
    kalmanFilterCenter.statePre.at<float>(0) = initial_mouse.first;  //-- x Position
    kalmanFilterCenter.statePre.at<float>(1) = initial_mouse.second; //-- y Position
    kalmanFilterCenter.statePre.at<float>(2) = 0;		     //-- x Velocity
    kalmanFilterCenter.statePre.at<float>(3) = 0;		     //-- y Velocity

    //-- Set the rest of the matrices:
    cv::setIdentity( kalmanFilterCenter.measurementMatrix );
    cv::setIdentity( kalmanFilterCenter.processNoiseCov, cv::Scalar::all(0.0001));
    cv::setIdentity( kalmanFilterCenter.measurementNoiseCov, cv::Scalar::all(0.1));
    cv::setIdentity( kalmanFilterCenter.errorCovPost, cv::Scalar::all(0.1));
}



//-----------------------------------------------------------------------------------------------------------------------
//-- Refresh the detected hand characteristics
//-----------------------------------------------------------------------------------------------------------------------
void HandDescription::operator ()(const cv::Mat& src, const cv::Mat& skinMask )
{
    update ( src, skinMask );
}

void HandDescription::update(const cv::Mat& src, const cv::Mat& skinMask )
{
    //-- Do things to update each parameter
    contourExtraction( skinMask );

    //-- Check if some hand was found:
    if ( _hand_found )
    {
        //-- Find bounding boxes
        boundingBoxExtraction(src);

        //-- Find hand palm
        handPalmExtraction(src);

        //-- Second roi and contourExtraction (optional right now)
        //! \todo Second ROI and contourExtraction on HandDescriptor
        ROIExtraction( skinMask );

        cv::Mat remaskedHand;
        cv::bitwise_and( skinMask, _hand_ROI, remaskedHand );
        contourExtraction( remaskedHand );

        //-- Find the min enclosing circle of the latest contour:
        cv::minEnclosingCircle( _hand_contour[0], _min_enclosing_circle_center, _min_enclosing_circle_radius );

        //-- Find the complex hull
        cv::convexHull( _hand_contour[0], _hand_hull );

        //-- Find convexity defects:
        std::vector<int> hull_indices;
        cv::convexHull( _hand_contour[0], hull_indices, CV_CLOCKWISE);
        cv::convexityDefects( _hand_contour[0], hull_indices, _hand_convexity_defects );

        angleExtraction();
        centerExtraction();


//	gestureExtraction(src);
//	numFingersExtraction();
    }
}

void HandDescription::gestureExtraction(const cv::Mat & src)
{
	//C++: void matchTemplate(InputArray image, InputArray templ, OutputArray result, int method)
	cv::Mat result; 
	cv::Mat templ=cv::imread("../data/hand1.jpg");

	matchTemplate (src,templ, result,0);
	std::string name="MatchTemplate";
	cv::imshow(name, result);	

}


//-----------------------------------------------------------------------------------------------------------------------
//-- Get the characteristics of the hand
//-----------------------------------------------------------------------------------------------------------------------

bool HandDescription::handFound()
{
    return _hand_found;
}

double HandDescription::getHandAngle ()
{
    return _hand_angle;
}

double HandDescription::getHandAnglePredicted()
{
    return _hand_angle_prediction;
}

double HandDescription::getHandAngleEstimated()
{
    return _hand_angle_estimation;
}

std::pair <int, int> HandDescription::getCenterHand ()
{
    return _hand_center;
}

std::pair <int, int> HandDescription::getCenterHandPredicted()
{
    return _hand_center_prediction;
}

std::pair <int, int> HandDescription::getCenterHandEstimated()
{
    return _hand_center_estimation;
}

std::vector< std::vector<cv::Point> > HandDescription::getContours()
{
    return _hand_contour;
}

cv::Rect HandDescription::getBoundingBox()
{
    return _hand_bounding_box;
}

int HandDescription::getGesture()
{
    return _hand_gesture;
}

int HandDescription::getNumFingers()
{
    return _hand_num_fingers;
}



//-----------------------------------------------------------------------------------------------------------------------
//-- Plot characteristics on some image
//-----------------------------------------------------------------------------------------------------------------------

//-- Draws the bounding rectangle of the ROI
void HandDescription::plotBoundingRectangle(const cv::Mat& src, cv::Mat& dst, bool rotated )
{
    //-- Allocate the display matrix if needed:
    if ( dst.total() == 0 )
	dst = src.clone();

    if ( _hand_found )
    {
        //-- Choose between rotated or std box:
        if ( rotated )
        {
            //-- Draw rotated rectangle:
            cv::Point2f rect_points[4]; _hand_rotated_bounding_box.points( rect_points );
            for( int j = 0; j < 4; j++ )
            cv::line( dst, rect_points[j], rect_points[(j+1)%4], cv::Scalar(255, 0, 0) , 2 );
        }
        else
        {
            cv::rectangle( dst, _hand_bounding_box, cv::Scalar( 255, 0, 0), 2 );
        }
    }
}


void HandDescription::plotContours(const cv::Mat& src, cv::Mat& dst)
{
    //-- Allocate the display matrix if needed:
    if ( dst.total() == 0 )
	dst = src.clone();

    //-- Plot contours:
    if ( _hand_found )
    {
	//-- Draw contours:
	cv::drawContours( dst, _hand_contour, 0, cv::Scalar( 0, 0, 255), 1, 8);
	//cv::fillConvexPoly( dst, contours[largestId], cv::Scalar( 255, 255, 255));
    }
    else
	std::cerr << "No contours found!" << std::endl;
}


void HandDescription::plotCenter(const cv::Mat& src, cv::Mat& dst, bool show_corrected, bool show_actual, bool show_predicted)
{
    //-- Allocate the display matrix if needed:
    if ( dst.total() == 0 )
	dst = src.clone();

    //-- Plot center
    if ( _hand_found )
    {
	if ( show_predicted )
	{
	    //-- Print predicted point on screen:
	    cv::Point predictedPoint( _hand_center_prediction.first, _hand_center_prediction.second );
	    cv::circle( dst, predictedPoint, 4, cv::Scalar( 0, 255, 0), 2 );
	}

	if ( show_actual )
	{
	    //-- Print cog on screen:
	    cv::Point cog( _hand_center.first, _hand_center.second );
	    cv::circle( dst, cog, 5, cv::Scalar( 255, 0, 0), 2 );
	}

	if ( show_corrected )
	{
	    //-- Print estimation on screen:
	    cv::Point estimationPoint( _hand_center_estimation.first, _hand_center_estimation.second );
	    cv::circle( dst, estimationPoint, 3, cv::Scalar( 0, 0, 255), 2 );
	}
    }

}

void HandDescription::plotMaxInscribedCircle( cv::Mat& src, cv::Mat& dst)
{
    if ( dst.empty() )
        dst = src.clone();

    if ( _hand_found )
    {
        cv::circle( dst, _max_circle_incribed_center, _max_circle_inscribed_radius, cv::Scalar(0, 255, 0));
        cv::circle( dst, _max_circle_incribed_center, 2, cv::Scalar( 0, 255, 0));
    }
}

void HandDescription::plotMinEnclosingCircle( cv::Mat& src, cv::Mat& dst)
{
    if ( dst.empty() )
        dst = src.clone();

    if ( _hand_found)
    {
        cv::circle( dst, _min_enclosing_circle_center, _min_enclosing_circle_radius, cv::Scalar(255, 0, 255));
        cv::circle( dst, _min_enclosing_circle_center, 2, cv::Scalar( 255, 0, 255));
    }
}

void HandDescription::plotComplexHull( cv::Mat& src, cv::Mat& dst, bool show_points)
{
    if ( dst.empty() )
        dst = src.clone();

    if ( _hand_found )
    {
        std::vector< std::vector < cv::Point > > points_to_show;
        points_to_show.push_back( _hand_hull);

        cv::drawContours( dst, points_to_show, 0, cv::Scalar( 0, 255, 255), 1, 8);

        if ( show_points )
            for (int i = 0; i < points_to_show[0].size(); i++)
                cv::circle( dst, points_to_show[0].at(i), 2, cv::Scalar( 0, 255, 255));

    }
}


void HandDescription::plotConvexityDefects(cv::Mat &src, cv::Mat &dst)
{
    if ( dst.empty() )
        dst = src.clone();

    if ( _hand_found )
    {
       /* for (int i = 0; i < _hand_convexity_defects.size(); i++)
            cv::circle( dst, _hand_convexity_defects.at(i), 2, cv::Scalar( 0, 140, 255));*/
    }
}

void HandDescription::angleControl(bool show_corrected, bool show_actual, bool show_predicted)
{
    //-- Prints the angle gauge

    //-- Matrix that shows the gauge
    cv::Mat gauge = cv::Mat::zeros( 100, 200, CV_8UC3);

    //-- Define gauge:
    int gauge_l = 60;
    cv::Point gaugeOrigin( 200/2, 80 );

    //-- Calculate and print actual end:
    if ( show_actual )
    {
	double rad_ang = _hand_angle * 3.1415 / 180.0;
	int x_coord = gauge_l * cos( rad_ang);
	int y_coord = gauge_l * sin( rad_ang);
	cv::Point gaugeEnd( 200/2 + x_coord , 80 - y_coord);
	cv::line( gauge, gaugeOrigin, gaugeEnd, cv::Scalar( 255, 0, 0), 1); //-- Actual angle
    }

    //-- Calculate and print predicted end:
    if ( show_predicted)
    {
	double rad_ang_predicted = _hand_angle_prediction * 3.1415 / 180.0;
	int x_coord_predicted = gauge_l * cos( rad_ang_predicted);
	int y_coord_predicted = gauge_l * sin( rad_ang_predicted);
	cv::Point predictedGaugeEnd( 200/2 + x_coord_predicted , 80 - y_coord_predicted);
	cv::line( gauge, gaugeOrigin, predictedGaugeEnd, cv::Scalar( 0, 255, 0), 2); //-- Predicted angle
    }

    //-- Calculate and print estimated end:
    if ( show_corrected )
    {
	double rad_ang_estimated = _hand_angle_estimation * 3.1415 / 180.0;
	int x_coord_estimated = gauge_l * cos( rad_ang_estimated);
	int y_coord_estimated = gauge_l * sin( rad_ang_estimated);
	cv::Point estimatedGaugeEnd( 200/2 + x_coord_estimated , 80 - y_coord_estimated);
	cv::line( gauge, gaugeOrigin, estimatedGaugeEnd, cv::Scalar( 0, 0, 255)), 3; //-- Estimated angle
    }

    cv::imshow( "Gauge", gauge);

}


//-----------------------------------------------------------------------------------------------------------------------
//-- Functions that extract characteristics:
//-----------------------------------------------------------------------------------------------------------------------

void HandDescription::contourExtraction(const cv::Mat& skinMask)
{
    const int epsilon = 3; //-- Max error for polygon approximation

    //-- Extract skin contours:
    std::vector<std::vector<cv::Point> > raw_contours;
    cv::findContours(skinMask, raw_contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, cv::Point(0,0) );

    //-- Filter the contours by size:
    std::vector<std::vector<cv::Point> > filtered_hand_contours;
    filterContours( raw_contours, filtered_hand_contours);

    //-- Set a flag in case no hands have been found:
    _hand_found = (int) filtered_hand_contours.size() > 0 ;

    //-- If contour was found, make a aproximation of them:
    _hand_contour = std::vector<std::vector<cv::Point > >( filtered_hand_contours.size() );

    if ( _hand_found )
        for( int i = 0; i < filtered_hand_contours.size(); i++)
            cv::approxPolyDP( filtered_hand_contours[i], _hand_contour[i], epsilon, True );
}

void HandDescription::boundingBoxExtraction(const cv::Mat& src)
{
    //-- Extract minimal rectangle enclosing the hand:
    _hand_rotated_bounding_box = cv::minAreaRect( _hand_contour[0]);

    //-- Extract bounding box:
    _hand_bounding_box  =  cv::boundingRect( _hand_contour[0] );

}

void HandDescription::handPalmExtraction(const cv::Mat& src )
{
    cv::Mat toPlot = src.clone();

    //-- Parameters describing this hand palm procedure:
    const int x_ratio = 3; //-- Section of the bounding box used for finding center (along X)
    const int y_ratio = 3; //-- Section of the bounding box used for finding center (along Y)

    const int step_contour = 1; //-- Test each 'step' points of the contour for easier calculations
    const int step_points_inside = 1;//-- Test each 'step' points inside the contour


    //-- Extract points to use
    std::vector< cv::Point > points_to_use;

    if ( step_contour == 1)
    {
        points_to_use = _hand_contour[0];
    }
    else
    {
        for (int i = 0; i < (int) ( _hand_contour[0].size() / (float) step_contour); i++)
            points_to_use.push_back( _hand_contour[0][i]);
    }

    //-- Find initial and ending points of the area to look for
    std::pair<int,int> x_limits, y_limits;

    x_limits.first = _hand_bounding_box.x + _hand_bounding_box.width /  x_ratio;
    x_limits.second = _hand_bounding_box.x + (int) ( _hand_bounding_box.width * (1 - 1 /(float) x_ratio));

    y_limits.first = _hand_bounding_box.y + _hand_bounding_box.height / y_ratio;
    y_limits.second = _hand_bounding_box.y + (int) ( _hand_bounding_box.height * (1 - 1 /(float) y_ratio));


    //-- Look for center and radius
    std::pair<int, int> best_center = std::pair<int, int>( x_limits.first, y_limits.first);
    double best_distance = -1;

    /*
    std::cout << "(" << x_limits.first << ", " << x_limits.second << ")" << std::endl;
    std::cout << "(" << y_limits.first << ", " << y_limits.second << ")" << std::endl;
    */

    for (int j = y_limits.first; j < y_limits.second; j += step_points_inside )
        for (int i = x_limits.first; i < x_limits.second; i += step_points_inside )
        {
            double current_distance = cv::pointPolygonTest( points_to_use, cv::Point(i, j), true );

            if (current_distance > 0 && current_distance > best_distance)
            {
                best_distance = current_distance;
                best_center.first = i;
                best_center.second = j;
            }
        }


    //-- Once best distance is found, we get the center and calculate the actual radius (only if something is found)
    if ( best_distance > -1 )
    {
        _max_circle_incribed_center = cv::Point( best_center.first, best_center.second );
        _max_circle_inscribed_radius = best_distance;
       // std::cout << "[Debug] Inscribed circle: " << _max_circle_incribed_center << " -> r =" << _max_circle_inscribed_radius << std::endl;
    }
    else
    {
        std::cerr << "[Error]: Inscribed circle could not be found!" << std::endl;
    }


    //--Plot circle for debugging
    plotContours(toPlot, toPlot);
    plotBoundingRectangle(toPlot, toPlot, false);
    plotMaxInscribedCircle( toPlot, toPlot);
    //cv::imshow( "[Debug] Inscribed circle", toPlot);
}

void HandDescription::ROIExtraction( const cv::Mat& src)
{
    //-- Pointers for writting less (and better reading)
    int * x = &(_max_circle_incribed_center.x);
    int * y = &(_max_circle_incribed_center.y);
    double * r = &(_max_circle_inscribed_radius);

    std::cout << "[Debug] ROI circle: Center at (" << *x << ", " << *y << ") R = " << *r << std::endl;

    //-- Extract hand ROI:
    int ROI_corner_x = (*x) - 3.5*(*r);
    int ROI_corner_y = (*y) - 3.5*(*r);
    int ROI_width, ROI_height;
    ROI_width = ROI_height = 7*(*r);

    //-- Check for limits:
    if (ROI_corner_x + ROI_width > src.cols)
        ROI_width = src.cols - ROI_corner_x;

    if (ROI_corner_y + ROI_height > src.rows)
        ROI_height = src.rows - ROI_corner_y;

    cv::Rect ROI_rectangle = cv::Rect( ROI_corner_x, ROI_corner_y, ROI_width, ROI_height );
    try
    {
        //_hand_ROI = src( ROI_rectangle ).clone();
        _hand_ROI = cv::Mat::zeros( src.size(), CV_8UC1);
        cv::rectangle( _hand_ROI, ROI_rectangle, cv::Scalar( 255, 255, 255), CV_FILLED);
    }
    catch( std::exception& e)
    {
        std::cerr << "An exception ocurred, but we are crossing fingers and ignoring it... :)" << std::endl;
    }

    //cv::imshow("[Debug] Hand", _hand_ROI);
}

void HandDescription::angleExtraction()
{
    //-- Create matrix for storing the measurement (measured position of hand)
    cv::Mat_<float> angleMeasurement(1, 1);
    angleMeasurement.setTo( cv::Scalar(0));

    //-- Predict angle with Kalman filter:
    cv::Mat anglePrediction = kalmanFilterAngle.predict();
    _hand_angle_prediction = anglePrediction.at<float>(0);

    //-- Measure actual angle:
    double newHandAngle = getAngle(_hand_rotated_bounding_box);
    _hand_angle = newHandAngle < 0 ? _hand_angle : newHandAngle;
    angleMeasurement(0) = _hand_angle;

    //-- Correct prediction:
    cv::Mat angleEstimation = kalmanFilterAngle.correct( angleMeasurement );
    _hand_angle_estimation = angleEstimation.at<float>(0);
}

void HandDescription::centerExtraction()
{
    //-- Predict next center position with kalman filter:
    cv::Mat prediction = kalmanFilterCenter.predict();
    _hand_center_prediction = std::pair <int, int> ( prediction.at<float>(0), prediction.at<float>(1) );

    //-- Measure actual point (uncomment the selected method):

    //-------------------- With RotatedRect --------------------------------------
//    cv::Point2f rect_points[4]; _hand_rotated_bounding_box.points( rect_points );
//    for( int j = 0; j < 4; j++ )
//    {
//	cog.first += rect_points[j].x;
//	cog.second += rect_points[j].y;
//    }

//    cog.first /= 4;
//    cog.second /= 4;


    //-------------------- With Bounding Rectangle --------------------------------
    //-- (This is more stable than the rotated rectangle)
    _hand_center.first  = _hand_bounding_box.x + _hand_bounding_box.width  / 2;
    _hand_center.second = _hand_bounding_box.y + _hand_bounding_box.height / 2;


    //-- Create matrix for storing the measurement (measured position of hand)
    cv::Mat_<float> measurement(2, 1);
    measurement(0) = _hand_center.first;
    measurement(1) = _hand_center.second;

    //-- Correct estimation:
    cv::Mat estimation = kalmanFilterCenter.correct( measurement);
    _hand_center_estimation = std::pair <int, int> ( estimation.at<float>(0), estimation.at<float>(1) );

}
